#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

#include "../tests/random.h"

template <typename SampleT, size_t N, double f0, double Fs>
class NoiseGenerator {

  static_assert(f0 <= Fs / 2);

public:
  explicit NoiseGenerator() noexcept {}

  void poll(std::vector<SampleT>& I_out) const { I_out = m_I; }

  void poll(std::vector<SampleT>& I_out, std::vector<SampleT>& Q_out) const {
    I_out = m_I;
    Q_out = m_Q;
  }

  static consteval size_t static_output_size() noexcept { return N; }

private:
  auto generate_I_tone() noexcept {
    std::vector<SampleT> I(N);
    for (size_t n{0}; n < N; ++n) {

      // Unit complex power noise:
      // Var(I) = Var(Q) = 0.5 -> E[|x|^2] = 1

      std::normal_distribution<SampleT> dist(
          static_cast<SampleT>(0), static_cast<SampleT>(std::sqrt(0.5)));

      I[n] = dist(Random::mt);
    }
    return I;
  }

  auto generate_Q_tone() noexcept {
    std::vector<SampleT> Q(N);
    for (size_t n{0}; n < N; ++n) {

      // Unit complex power noise:
      // Var(I) = Var(Q) = 0.5 -> E[|x|^2] = 1

      std::normal_distribution<SampleT> dist(
          static_cast<SampleT>(0), static_cast<SampleT>(std::sqrt(0.5)));

      Q[n] = dist(Random::mt);
    }
    return Q;
  }

  std::vector<SampleT> m_I{generate_I_tone()};
  std::vector<SampleT> m_Q{generate_Q_tone()};
};
