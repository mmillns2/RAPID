#pragma once

#include <concepts>
#include <vector>

template <typename T>
concept FEDigBase = requires(T t) {
  { T::static_output_size() } -> std::convertible_to<size_t>;
  { T::get_static_prescale() } -> std::convertible_to<uint32_t>;
  { T::static_accumulator_size() } -> std::convertible_to<uint32_t>;
  { T::static_accumulator_norm() } -> std::convertible_to<bool>;
};

template <typename T, typename SampleT>
concept ImagDig = FEDigBase<T> && requires(T t, uint64_t &timestamp, std::vector<SampleT> &I_out,
                                           std::vector<SampleT> &Q_out) {
  { t.poll(timestamp, I_out, Q_out) } -> std::same_as<void>;
};

template <typename T, typename SampleT>
concept RealDig = FEDigBase<T> && requires(T t, uint64_t &timestamp, std::vector<SampleT> &out) {
  { t.poll(timestamp, out) } -> std::same_as<void>;
};

template <typename T, typename SampleT>
concept FEDig = ImagDig<T, SampleT> || RealDig<T, SampleT>;
