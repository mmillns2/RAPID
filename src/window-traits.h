#pragma once

#include <array>
#include <concepts>

template <typename W, size_t N>
concept WindowType = requires {
  { W::template make<N>() } -> std::same_as<std::array<double, N>>;
};
