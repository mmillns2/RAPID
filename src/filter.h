#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
// #include <print>
#include <span>

#include "window-traits.h"

template <typename SampleT, size_t Taps, size_t Decimation, double FcHz,
          double FsHz, size_t InputSize, typename Window>
class DecimatingFIR {

  static_assert(WindowType<Window, Taps>, "Invalid Window type");

  static_assert(std::has_single_bit(Taps), "taps must be power of two");
  static_assert(FcHz > 0.0);
  static_assert(FcHz < FsHz / 2.0);
  static_assert(FcHz < FsHz / (2.0 * Decimation),
                "cutoff too high for decimation (aliasing risk)");

  static constexpr double FcNorm{FcHz / FsHz};
  static constexpr size_t Mask{Taps - 1};

  static constexpr double sinc(double x) noexcept {
    if (x == 0.0)
      return 1.0;

    return std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
  }

  static consteval auto make_taps() {

    std::array<SampleT, Taps> taps{};
    constexpr size_t M{Taps - 1};

    constexpr auto window{Window::template make<Taps>()};

    for (size_t n{}; n < Taps; ++n) {

      const double centered{static_cast<double>(n) -
                            static_cast<double>(M) / 2.0};

      const double ideal{2.0 * FcNorm * sinc(2.0 * FcNorm * centered)};

      double value{ideal * window[n]};
      taps[n] = static_cast<SampleT>(value);
    }

    return taps;
  }

  static constexpr auto s_taps{make_taps()};

  std::array<SampleT, Taps> m_delay_I{};
  std::array<SampleT, Taps> m_delay_Q{};

  size_t m_write_pos{};
  size_t m_phase_counter{};

public:
  constexpr DecimatingFIR() noexcept = default;

  void reset() noexcept {
    m_delay_I.fill(0.0);
    m_delay_Q.fill(0.0);
    m_write_pos = 0;
    m_phase_counter = 0;
  }

  void process_old(std::span<const SampleT> I_in, std::span<const SampleT> Q_in,
                   std::span<SampleT> I_out,
                   std::span<SampleT> Q_out) noexcept {
    const size_t N{I_in.size()};
    size_t out_index{};

    for (size_t n{}; n < N; ++n) {
      // Write into circular buffer
      m_delay_I[m_write_pos] = I_in[n];
      m_delay_Q[m_write_pos] = Q_in[n];

      m_write_pos = (m_write_pos + 1) & Mask;

      // Decimation counter
      if (++m_phase_counter == Decimation) {
        m_phase_counter = 0;

        double acc_I{};
        double acc_Q{};

        size_t idx{m_write_pos};

        // Full convolution (all taps used)
        for (size_t k{}; k < Taps; ++k) {
          idx = (idx - 1) & Mask;

          acc_I += s_taps[k] * m_delay_I[idx];
          acc_Q += s_taps[k] * m_delay_Q[idx];
        }

        I_out[out_index] = static_cast<SampleT>(acc_I);
        Q_out[out_index] = static_cast<SampleT>(acc_Q);

        ++out_index;
      }
    }
  }

  void process(std::span<const SampleT> I_in, std::span<const SampleT> Q_in,
               std::span<SampleT> I_out, std::span<SampleT> Q_out) noexcept {

    // std::println("Filter: processing");

    const size_t N = I_in.size();
    // std::cout << "Filter: I: " << N << ", Q: " << Q_in.size() << '\n';
    // assert(N == Q_in.size());

    size_t out_index = 0;

    for (size_t n = 0; n < N; ++n) {
      // Write newest sample
      m_delay_I[m_write_pos] = I_in[n];
      m_delay_Q[m_write_pos] = Q_in[n];

      m_write_pos = (m_write_pos + 1) & (Taps - 1);

      if (++m_phase_counter == Decimation) {
        m_phase_counter = 0;

        SampleT acc_I{};
        SampleT acc_Q{};

        size_t wp = m_write_pos;

        size_t first = Taps - wp; // tail part
        size_t second = wp;       // head part

#pragma GCC ivdep
        for (size_t k = 0; k < first; ++k) {
          acc_I += s_taps[k] * m_delay_I[wp + k];
          acc_Q += s_taps[k] * m_delay_Q[wp + k];
        }

#pragma GCC ivdep
        for (size_t k = 0; k < second; ++k) {
          acc_I += s_taps[first + k] * m_delay_I[k];
          acc_Q += s_taps[first + k] * m_delay_Q[k];
        }

        I_out[out_index] = acc_I;
        Q_out[out_index] = acc_Q;
        ++out_index;
      }
    }
  }

  static consteval size_t static_output_size() noexcept {
    return InputSize / Decimation;
  }

  static consteval size_t static_input_size() noexcept { return InputSize; }

  size_t output_size(size_t input_size) const noexcept {
    return (m_phase_counter + input_size) / Decimation;
  }
};
