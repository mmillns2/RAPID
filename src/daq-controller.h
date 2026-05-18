#pragma once

#include "daq-stage-traits.h"
#include "dsp-controller-traits.h"
#include "dsp-controller.h"
#include "dsp-stage-traits.h"
#include "frontend-traits.h"
#include "midas-traits.h"
#include "spsc-queue.h"
#include "stage-snapshot.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <thread>
#include <tuple>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

template <typename SampleT, typename StoreT, typename... Stages>
class DAQController {
public:
  using buffer_pair = std::pair<std::vector<SampleT>, std::vector<SampleT>>;
  using buffer_ptr = std::unique_ptr<buffer_pair>;
  using Snapshot = StageSnapshot<SampleT>;
  using SnapshotPtr = snapshot_ptr<SampleT>;

private:
  static_assert(sizeof...(Stages) >= 1,
                "DAQController requires at least a digitiser");

  static constexpr std::size_t N{sizeof...(Stages)};

  static_assert((DAQStage<Stages> && ...),
                "All DAQ stages must be a DAQStage (daq-stage-traits).");

  static_assert(ValidSizes_v<Stages...>,
                "Invalid DAQ pipeline: the output size of stage n must equal "
                "the input size of stage n+1");

  // Queue depths
  static constexpr std::size_t k_data_depth{64};  // 2^6
  static constexpr std::size_t k_free_depth{128}; // 2^7
  static constexpr std::size_t k_tap_depth{64};   // 2^6

  static_assert((k_data_depth & (k_data_depth - 1)) == 0);
  static_assert((k_free_depth & (k_free_depth - 1)) == 0);
  static_assert((k_tap_depth & (k_tap_depth - 1)) == 0);
  static_assert(k_free_depth >= k_data_depth + 2);

  // Queue types
  using data_value_t = std::pair<uint64_t, buffer_ptr>;
  using data_queue_t = SPSCQueue<data_value_t>;
  using free_queue_t = SPSCQueue<buffer_ptr>;
  using tap_queue_t = SPSCQueue<SnapshotPtr>;
  using data_queue_ptr = std::unique_ptr<data_queue_t>;
  using free_queue_ptr = std::unique_ptr<free_queue_t>;
  using tap_queue_ptr = std::unique_ptr<tap_queue_t>;

  // N queues, one per stage. data_queue[N-1] is consumed by MIDAS.
  std::array<data_queue_ptr, N> m_data_queues;
  std::array<free_queue_ptr, N> m_free_queues;
  std::array<tap_queue_ptr, N> m_tap_queues;

  std::array<uint32_t, N> m_prescale_counter{};

  static constexpr std::size_t k_cache_line{64};
  struct alignas(k_cache_line) PaddedAtomic {
    std::atomic<bool> value{false};
  };
  static_assert(sizeof(PaddedAtomic) == k_cache_line);
  PaddedAtomic m_running;

  StageTuple_t<Stages...> m_stages;
  std::vector<std::thread> m_threads;

  std::atomic<uint64_t> m_polls{0};
  std::atomic<uint64_t> m_taps{0};

  // CPU affinity
  static void pin_thread_to_core(unsigned core) {
#if defined(__linux__)
    const unsigned nproc{std::thread::hardware_concurrency()};
    if (nproc == 0)
      return;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core % nproc, &cpuset);
    (void)pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#else
    (void)core;
#endif
  }

  // Initialisation
  template <std::size_t... Is> void init_queues(std::index_sequence<Is...>) {
    ((m_data_queues[Is] = std::make_unique<data_queue_t>(k_data_depth),
      m_free_queues[Is] = std::make_unique<free_queue_t>(k_free_depth),
      m_tap_queues[Is] = std::make_unique<tap_queue_t>(k_tap_depth)),
     ...);
  }

  template <std::size_t I> void prefill_free_queue() {
    using Stage = StageElement_t<I, Stages...>;
    static constexpr std::size_t i_size{Stage::static_output_size()};
    static constexpr std::size_t q_size{
        ComplexDAQStage_v<SampleT, Stage> ? i_size : 0};
    auto &fq = *m_free_queues[I];
    for (std::size_t n{0}; n < k_free_depth; ++n) {
      [[maybe_unused]] bool ok{fq.push(std::make_unique<buffer_pair>(
          std::vector<SampleT>(i_size), std::vector<SampleT>(q_size)))};
      assert(ok && "free_queue underflow at prefill");
    }
  }

  template <std::size_t... Is>
  void prefill_free_queues(std::index_sequence<Is...>) {
    (prefill_free_queue<Is>(), ...);
  }

  // Buffer pool accessors
  template <std::size_t I> buffer_ptr get_buffer() {
    auto &fq = *m_free_queues[I];
    buffer_ptr buf;
    while (!fq.pop(buf))
      if (!m_running.value.load(std::memory_order_relaxed))
        return nullptr;
    return buf;
  }

  template <std::size_t I> void recycle_buffer(buffer_ptr buf) {
    [[maybe_unused]] bool ok{m_free_queues[I]->push(std::move(buf))};
    assert(ok && "free_queue overflow on recycle");
  }

  // Prescale value is read from the stage type at compile time.
  // Counter is a runtime member so different DAQController instances
  // have independent counts.
  template <std::size_t I> void maybe_tap(const buffer_ptr &buf, uint64_t ts) {
    using Stage = StageElement_t<I, Stages...>;
    constexpr uint32_t prescale{Stage::get_static_prescale()};
    if constexpr (prescale == 0)
      return; // disabled -> zero overhead

    auto &counter = m_prescale_counter[I];
    if (++counter < prescale)
      return;
    counter = 0;

    auto snap = std::make_shared<Snapshot>();
    snap->stage_id = static_cast<uint8_t>(I);
    snap->timestamp = ts;
    snap->I = buf->first;
    if constexpr (ComplexDAQStage_v<SampleT, Stage>)
      snap->Q = buf->second;

    // Drop snapshot if tap queue is full
    if (!m_tap_queues[I]->push(std::move(snap)))
      --counter;
    else
      m_taps.fetch_add(1, std::memory_order_relaxed);
  }

  // Per stage thread loop
  template <std::size_t I> void stage_loop(unsigned cpu_offset) {
    pin_thread_to_core(cpu_offset + static_cast<unsigned>(I));

    using Stage = StageElement_t<I, Stages...>;
    auto &stage = std::get<I>(m_stages);

    if constexpr (FEDig<Stage, SampleT>) {
      // Digitiser: producer only
      auto &dq = *m_data_queues[I];

      while (m_running.value.load(std::memory_order_relaxed)) {
        buffer_ptr buf{get_buffer<I>()};
        if (!buf)
          return;

        uint64_t ts{0};
        if constexpr (ImagDig<Stage, SampleT>)
          stage.poll(ts, buf->first, buf->second);
        else
          stage.poll(ts, buf->first);

        m_polls.fetch_add(1, std::memory_order_relaxed);

        // When the digitiser is the final stage (N=1), fill_midas_event writes
        // directly from the pipeline buffer (no snapshot copy needed).
        if constexpr (I < N - 1)
          maybe_tap<I>(buf, ts);

        data_value_t slot{ts, std::move(buf)};
        while (!dq.push(std::move(slot)))
          if (!m_running.value.load(std::memory_order_relaxed))
            return;
      }

    } else if constexpr (is_DSPController_v<Stage>) {
      // DSP stage: consumer + producer
      constexpr uint32_t K{Stage::static_accumulator_size()};
      constexpr bool norm{Stage::static_accumulator_norm()};

      static_assert(K >= 1, "static_accumulator_size() must be >= 1");

      auto &in_dq = *m_data_queues[I - 1];
      auto &out_dq = *m_data_queues[I];

      // Accumulator state
      buffer_ptr accum_buf; // null until first frame of each window
      uint32_t accum_count{0};
      uint64_t first_ts{0};

      while (m_running.value.load(std::memory_order_relaxed)) {
        data_value_t item;
        if (!in_dq.pop(item))
          continue;

        buffer_ptr out_buf{get_buffer<I>()};
        if (!out_buf) {
          recycle_buffer<I - 1>(std::move(item.second));
          return;
        }

        // Zero out_buf so DSP stages that accumulate with +=
        // (e.g. WelchPSD) start each frame clean.
        std::fill(out_buf->first.begin(), out_buf->first.end(), SampleT{});
        std::fill(out_buf->second.begin(), out_buf->second.end(), SampleT{});

        if constexpr (is_ImagDSPController_v<Stage>)
          stage.process(item.second->first, item.second->second, out_buf->first,
                        out_buf->second);
        else
          stage.process(item.second->first, item.second->second,
                        out_buf->first);

        // Return the input buffer as early as possible so the
        // upstream stage can reuse it immediately.
        recycle_buffer<I - 1>(std::move(item.second));

        if constexpr (K <= 1) {
          if constexpr (I < N - 1)
            maybe_tap<I>(out_buf, item.first);

          data_value_t out_slot{item.first, std::move(out_buf)};
          while (!out_dq.push(std::move(out_slot)))
            if (!m_running.value.load(std::memory_order_relaxed))
              return;

        } else { // K > 1: accumulation path
          if (accum_count == 0) {
            // First frame of a new window: out_buf becomes the accumulator with
            // a simple ownership move (no element wise copy).
            accum_buf = std::move(out_buf);
            first_ts = item.first;
            accum_count = 1;
          } else {
            // Subsequent frames: add out_buf into accum_buf element-wise, then
            // recycle out_buf.
            const std::size_t ni{accum_buf->first.size()};
            for (std::size_t k{0}; k < ni; ++k)
              accum_buf->first[k] += out_buf->first[k];

            if constexpr (ComplexDAQStage_v<SampleT, Stage>) {
              const std::size_t nq{accum_buf->second.size()};
              for (std::size_t k{0}; k < nq; ++k)
                accum_buf->second[k] += out_buf->second[k];
            }

            // Return the temporary output buffer to this stage's own free pool.
            recycle_buffer<I>(std::move(out_buf));
            ++accum_count;
          }

          if (accum_count == K) {
            // Window complete: normalise if requested.
            if constexpr (norm) {
              constexpr SampleT inv_K{SampleT{1} / static_cast<SampleT>(K)};
              for (auto &v : accum_buf->first)
                v *= inv_K;
              if constexpr (ComplexDAQStage_v<SampleT, Stage>)
                for (auto &v : accum_buf->second)
                  v *= inv_K;
            }

            // Tap the completed window for prescale snapshots.
            if constexpr (I < N - 1)
              maybe_tap<I>(accum_buf, first_ts);

            // Push accumulated result downstream.
            data_value_t out_slot{first_ts, std::move(accum_buf)};
            while (!out_dq.push(std::move(out_slot)))
              if (!m_running.value.load(std::memory_order_relaxed))
                return;

            // Reset for the next window. accum_buf is null (moved above), it is
            // reacquired on the next frame's accum_count == 0 branch.
            accum_count = 0;
          }
        }
      }
    }
  }

  // Thread launch
  template <std::size_t I> void start_stage_thread(unsigned cpu_offset) {
    m_threads.emplace_back([this, cpu_offset] { stage_loop<I>(cpu_offset); });
  }

  template <std::size_t... Is>
  void start_threads(unsigned cpu_offset, std::index_sequence<Is...>) {
    (start_stage_thread<Is>(cpu_offset), ...);
  }

  // MIDAS bank writing helpers (called from fill_midas_event, MIDAS thread)
  template <std::size_t I>
  static void write_channel_bank(char *pevent, const char *name,
                                 std::span<const SampleT> data) {
    constexpr int tid{midas_tid<SampleT>::value};

    if constexpr (tid == midas_tid<StoreT>::value &&
                  !std::is_same_v<SampleT, StoreT>) {
      StoreT *ptr{nullptr};
      bk_create(pevent, name, tid, reinterpret_cast<void **>(&ptr));
      for (SampleT s : data)
        *ptr++ = static_cast<StoreT>(s);
      bk_close(pevent, ptr);
    } else { // SampleT == StoreT - ~4-8x faster
      SampleT *ptr{nullptr};
      bk_create(pevent, name, tid, reinterpret_cast<void **>(&ptr));
      std::memcpy(ptr, data.data(), data.size_bytes());
      ptr += data.size();
      bk_close(pevent, ptr);
    }
  }

  // Write all banks for one snapshot.
  template <std::size_t I>
  static void write_snapshot_banks(char *pevent, const Snapshot &snap) {
    // Timestamp bank
    constexpr auto ts_name{make_bank_name('T', 's', I)};
    uint64_t *pts{nullptr};
    bk_create(pevent, ts_name.data(), TID_UINT64,
              reinterpret_cast<void **>(&pts));
    *pts++ = snap.timestamp;
    bk_close(pevent, pts);

    // I-channel bank
    constexpr auto si_name{make_bank_name('S', 'i', I)};
    write_channel_bank<I>(pevent, si_name.data(), snap.I);

    // Q-channel bank (only if this stage produces Q)
    if (snap.has_q()) {
      constexpr auto sq_name = make_bank_name('S', 'q', I);
      write_channel_bank<I>(pevent, sq_name.data(), snap.Q);
    }
  }

  // Drain one snapshot from tap_queue[I] and write it if present.
  // Returns true if a snapshot was written.
  template <std::size_t I> bool drain_tap_to_midas(char *pevent) {
    using Stage = StageElement_t<I, Stages...>;
    constexpr uint32_t prescale{Stage::get_static_prescale()};
    if constexpr (prescale == 0)
      return false;

    SnapshotPtr snap;
    if (!m_tap_queues[I]->pop(snap) || !snap)
      return false;

    write_snapshot_banks<I>(pevent, *snap);
    return true;
  }

  // Drain tap queues for upstream stages 0..N-2 only.
  // Stage N-1 is written directly from the pipeline buffer -> no tap queue.
  template <std::size_t... Is>
  void drain_upstream_taps(char *pevent, std::index_sequence<Is...>) {
    (drain_tap_to_midas<Is>(pevent), ...);
  }

  // Write stage I's banks directly from the pipeline buffer (zero snapshot
  // copies). The prescale counter lives here.
  template <std::size_t I>
  void write_final_stage_banks(char *pevent, const buffer_ptr &buf,
                               uint64_t ts) {
    using Stage = StageElement_t<I, Stages...>;
    constexpr uint32_t prescale{Stage::get_static_prescale()};
    if constexpr (prescale == 0)
      return;

    auto &counter = m_prescale_counter[I];
    if (++counter < prescale)
      return;
    counter = 0;

    // Timestamp bank
    constexpr auto ts_name = make_bank_name('T', 's', I);
    uint64_t *pts{nullptr};
    bk_create(pevent, ts_name.data(), TID_UINT64,
              reinterpret_cast<void **>(&pts));
    *pts++ = ts;
    bk_close(pevent, pts);

    // I-channel: written directly from the pipeline buffer -> no copy
    constexpr auto si_name = make_bank_name('S', 'i', I);
    write_channel_bank<I>(pevent, si_name.data(), buf->first);

    // Q-channel (only if stage is complex)
    if constexpr (ComplexDAQStage_v<SampleT, Stage>) {
      constexpr auto sq_name = make_bank_name('S', 'q', I);
      write_channel_bank<I>(pevent, sq_name.data(), buf->second);
    }
  }

public:
  template <typename... Args>
  explicit DAQController(Args &&...args)
      : m_stages(std::forward<Args>(args)...) {
    init_queues(std::make_index_sequence<N>{});
    prefill_free_queues(std::make_index_sequence<N>{});
  }

  ~DAQController() { stop(); }

  DAQController(const DAQController &) = delete;
  DAQController &operator=(const DAQController &) = delete;
  DAQController(DAQController &&) = delete;
  DAQController &operator=(DAQController &&) = delete;

  void run(unsigned cpu_offset = 0) {
    m_running.value.store(true, std::memory_order_seq_cst);
    m_threads.reserve(N);
    start_threads(cpu_offset, std::make_index_sequence<N>{});
  }

  void stop() {
    m_running.value.store(false, std::memory_order_seq_cst);
    for (auto &t : m_threads)
      if (t.joinable())
        t.join();
    m_threads.clear();
    std::cout << "DAQController: polls=" << m_polls.load() << ", "
              << "taps=" << m_taps.load() << '\n';
  }

  // MIDAS integration - called from the MIDAS frontend thread
  bool has_event() const { return !m_data_queues[N - 1]->empty(); }

  INT fill_midas_event(char *pevent) {
    data_value_t item;
    if (!m_data_queues[N - 1]->pop(item))
      return 0;

    bk_init32(pevent);

    write_final_stage_banks<N - 1>(pevent, item.second, item.first);

    // Return the buffer to the producer immediately.
    recycle_buffer<N - 1>(std::move(item.second));

    // Drain upstream stages (0..N-2) from their tap queues.
    if constexpr (N > 1)
      drain_upstream_taps(pevent, std::make_index_sequence<N - 1>{});

    return bk_size(pevent);
  }
};
