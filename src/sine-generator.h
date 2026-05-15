#pragma once

#include <cassert>
#include <chrono>
#include <cmath>
#include <numbers>
//#include <print>
#include <span>
#include <thread>
#include <vector>

template <typename SampleT, size_t N, double f0, double Fs>
class SineGenerator {

  static_assert(f0 <= Fs / 2);

public:
  explicit SineGenerator() noexcept {}

  void poll(uint64_t &ts, std::vector<SampleT> &I_out) const { I_out = m_I; }

  void poll(uint64_t &ts, std::vector<SampleT> &I_out,
            std::vector<SampleT> &Q_out) const {
    //std::println("SineGenerator: polling");
    I_out = m_I;
    Q_out = m_Q;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  static consteval size_t static_output_size() noexcept { return N; }

private:
  auto generate_I_tone() noexcept {
    std::vector<SampleT> I(N);
    for (size_t n{0}; n < N; ++n) {
      double phase{2.0 * std::numbers::pi * f0 * n / Fs};
      I[n] = std::cos(phase);
    }
    return I;
  }

  auto generate_Q_tone() noexcept {
    std::vector<SampleT> Q(N);
    for (size_t n{0}; n < N; ++n) {
      double phase{2.0 * std::numbers::pi * f0 * n / Fs};
      Q[n] = std::sin(phase);
    }
    return Q;
  }

  std::vector<SampleT> m_I{generate_I_tone()};
  std::vector<SampleT> m_Q{generate_Q_tone()};
};
