#pragma once

#include <array>
#include <iostream>
#include <bit>
#include <cmath>
#include <cstddef>
#include <numbers>
//#include <print>
#include <span>
#include <cstdint>

#include "fft.h"
#include "window-traits.h"

template <typename SampleT, size_t FFTSize, size_t Overlap, double FsHz,
          size_t InputSize, typename Window>
class WelchPSD {

  static_assert(WindowType<Window, FFTSize>, "Invalid Window type");
  static_assert(Overlap < FFTSize, "Overlap must be < FFTSize");

  static constexpr size_t Step{FFTSize - Overlap};

  static constexpr auto s_window{Window::template make<FFTSize>()};

  static consteval double compute_window_norm() {
    double sum{};
    for (size_t i{}; i < FFTSize; ++i)
      sum += s_window[i] * s_window[i];
    return sum / FFTSize;
  }

  static constexpr SampleT s_window_norm{
      static_cast<SampleT>(compute_window_norm())};

  FFTWrapper<SampleT, FFTSize> m_fft{};

  std::array<SampleT, FFTSize> m_segment_I{};
  std::array<SampleT, FFTSize> m_segment_Q{};

  std::array<SampleT, FFTSize> m_fft_I{};
  std::array<SampleT, FFTSize> m_fft_Q{};

public:
  constexpr WelchPSD() = default;

  void reset() noexcept {
    m_segment_I.fill(static_cast<SampleT>(0));
    m_segment_Q.fill(static_cast<SampleT>(0));
  }

  void process(std::span<const SampleT> I_in, std::span<const SampleT> Q_in,
               std::span<SampleT> PSD_out) {

    //std::println("PSD: processing");

    if (Q_in.size() != InputSize || PSD_out.size() < FFTSize)
      throw std::runtime_error("WelchPSD: span mismatch");

    size_t segment_count{0};

    for (size_t start{0}; start + FFTSize <= InputSize; start += Step) {

      // Window into segment buffers
      for (size_t i{0}; i < FFTSize; ++i) {
        m_segment_I[i] = I_in[start + i] * static_cast<SampleT>(s_window[i]);

        m_segment_Q[i] = Q_in[start + i] * static_cast<SampleT>(s_window[i]);
      }

      // FFT
      m_fft.process(m_segment_I, m_segment_Q, m_fft_I, m_fft_Q);

      // Accumulate power
      for (size_t k{0}; k < FFTSize; ++k) {
        const SampleT re{m_fft_I[k]};
        const SampleT im{m_fft_Q[k]};

        PSD_out[k] += re * re + im * im;
      }

      ++segment_count;

      //std::cout << "psd\n";
    }

    if (segment_count == 0)
      return;

    const SampleT scale{static_cast<SampleT>(
        1.0 / (segment_count * s_window_norm * FFTSize * FsHz))};

    for (size_t k{0}; k < FFTSize; ++k)
      PSD_out[k] *= scale;
  }

  static constexpr double bin_frequency(size_t k) noexcept {
    return (static_cast<double>(k) * FsHz) / static_cast<double>(FFTSize);
  }

  static consteval size_t static_output_size() noexcept { return FFTSize; }

  static consteval size_t static_input_size() noexcept { return InputSize; }

};
