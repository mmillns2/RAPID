#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <new>

/*
Code taken from https://github.com/CharlesFrasch/cppcon2023/

SPSCQueue: single-producer / single-consumer lock-free ring buffer.

T may be any move-constructible, move-assignable type, including
move-only types such as std::unique_ptr. Copy-based push/pop overloads
are also provided for backward compatibility when T is copyable.

Thread safety:
  push() must be called from exactly one thread (the producer).
  pop()  must be called from exactly one thread (the consumer).
  Producer and consumer may be different threads simultaneously.

Memory layout:
  Each of the four hot fields (pushCursor, popCursorCached, popCursor,
  pushCursorCached) occupies its own cache line to eliminate false sharing
  between the producer and consumer threads.
*/

template <typename T, typename Alloc = std::allocator<T>>
class SPSCQueue : private Alloc {
public:
  using allocator_traits = std::allocator_traits<Alloc>;
  using size_type = typename allocator_traits::size_type;

  explicit SPSCQueue(size_type capacity = 16, Alloc const &alloc = Alloc{})
      : Alloc{alloc}, m_mask{capacity - 1},
        m_ring{allocator_traits::allocate(*this, capacity)} {}

  ~SPSCQueue() {
    while (!empty()) {
      element(m_popCursor)->~T();
      ++m_popCursor;
    }
    allocator_traits::deallocate(*this, m_ring, capacity());
  }

  // Non-copyable, non-movable: holds raw allocated memory and atomics.
  SPSCQueue(const SPSCQueue &) = delete;
  SPSCQueue &operator=(const SPSCQueue &) = delete;
  SPSCQueue(SPSCQueue &&) = delete;
  SPSCQueue &operator=(SPSCQueue &&) = delete;

  auto size() const noexcept {
    auto push = m_pushCursor.load(std::memory_order_relaxed);
    auto pop = m_popCursor.load(std::memory_order_relaxed);
    assert(pop <= push);
    return push - pop;
  }

  auto empty() const noexcept { return size() == 0; }
  auto full() const noexcept { return size() == capacity(); }
  auto capacity() const noexcept { return m_mask + 1; }

  // push — producer thread only
  /// Push by copy (T must be copy-constructible).
  /// Returns true on success, false if the queue is full.
  bool push(const T &value) {
    auto pushCursor = m_pushCursor.load(std::memory_order_relaxed);
    if (full(pushCursor, m_popCursorCached)) {
      m_popCursorCached = m_popCursor.load(std::memory_order_acquire);
      if (full(pushCursor, m_popCursorCached))
        return false;
    }
    new (element(pushCursor)) T(value);
    m_pushCursor.store(pushCursor + 1, std::memory_order_release);
    return true;
  }

  /// Push by move (T need only be move-constructible — works for unique_ptr).
  /// Returns true on success, false if the queue is full.
  /// On failure, `value` has been moved-from only if the queue accepts it,
  /// so the caller's object is valid (though unspecified) on false return.
  bool push(T &&value) {
    auto pushCursor = m_pushCursor.load(std::memory_order_relaxed);
    if (full(pushCursor, m_popCursorCached)) {
      m_popCursorCached = m_popCursor.load(std::memory_order_acquire);
      if (full(pushCursor, m_popCursorCached))
        return false;
    }
    new (element(pushCursor)) T(std::move(value));
    m_pushCursor.store(pushCursor + 1, std::memory_order_release);
    return true;
  }

  // pop: consumer thread only
  /// Pop by move-assign into value (works for move-only types).
  /// The ring slot is destroyed immediately after the move so the buffer
  /// never holds a zombie object.
  /// Returns true on success, false if the queue is empty.
  bool pop(T &value) {
    auto popCursor = m_popCursor.load(std::memory_order_relaxed);
    if (empty(m_pushCursorCached, popCursor)) {
      m_pushCursorCached = m_pushCursor.load(std::memory_order_acquire);
      if (empty(m_pushCursorCached, popCursor))
        return false;
    }
    value = std::move(*element(popCursor));
    element(popCursor)->~T();
    m_popCursor.store(popCursor + 1, std::memory_order_release);
    return true;
  }

private:
  bool full(size_type push, size_type pop) const noexcept {
    return (push - pop) == capacity();
  }

  static bool empty(size_type push, size_type pop) noexcept {
    return push == pop;
  }

  T *element(size_type cursor) noexcept { return &m_ring[cursor & m_mask]; }

  // Data members: each on its own cache line to prevent false sharing
  size_type m_mask;
  T *m_ring;

  using CursorType = std::atomic<size_type>;
  static_assert(CursorType::is_always_lock_free,
                "atomic<size_type> must be lock free on this platform");

  static constexpr size_type k_cache_line = 64;

  /// Written by producer, read by consumer.
  alignas(k_cache_line) CursorType m_pushCursor{};

  /// Private to the producer: cached snapshot of popCursor.
  alignas(k_cache_line) size_type m_popCursorCached{};

  /// Written by consumer, read by producer.
  alignas(k_cache_line) CursorType m_popCursor{};

  /// Private to the consumer: cached snapshot of pushCursor.
  alignas(k_cache_line) size_type m_pushCursorCached{};

  // Pad the last field to a full cache line so the queue object itself
  // does not share a line with whatever follows it in memory.
  char m_padding[k_cache_line - sizeof(size_type)];
};
