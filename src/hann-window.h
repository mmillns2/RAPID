#pragma once

#include <cmath>
#include <numbers>

struct HannWindow {

  template <size_t N> static consteval std::array<double, N> make() {

    std::array<double, N> w{};

    for (size_t n{}; n < N; ++n) {
      w[n] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * n / (N - 1));
    }

    return w;
  }
};
