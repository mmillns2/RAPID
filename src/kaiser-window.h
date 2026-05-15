#pragma once

#include <array>
#include <cmath>

constexpr double factorial(size_t n) {
  double result{1.0};
  for (size_t i{2}; i <= n; ++i)
    result *= static_cast<double>(i);
  return result;
}

constexpr double bessel_I0(double x) {

  constexpr size_t terms{20};

  double sum{1.0};
  double x_half_sq{(x * x) / 4.0};

  for (size_t k{1}; k < terms; ++k) {
    double denom{factorial(k)};
    double term{std::pow(x_half_sq, k) / (denom * denom)};
    sum += term;
  }

  return sum;
}

template <double Beta> struct KaiserWindow {

  template <size_t N> static consteval std::array<double, N> make() {

    std::array<double, N> w{};

    constexpr size_t M{N - 1};
    const double denom{bessel_I0(Beta)};

    for (size_t n{}; n < N; ++n) {

      const double x{(2.0 * n) / static_cast<double>(M) - 1.0};

      w[n] = bessel_I0(Beta * std::sqrt(1.0 - x * x)) / denom;
    }

    return w;
  }
};
