#pragma once

#include <array>
#include <cmath>
#include <numbers>

struct HammingWindow {

  template <size_t N> static consteval std::array<double, N> make() {

    std::array<double, N> w{};

    constexpr size_t M{N - 1};

    for (size_t n{}; n < N; ++n) {
      w[n] = 0.54 - 0.46 * std::cos(2.0 * std::numbers::pi * n / M);
    }

    return w;
  }
};
