#pragma once

#include <array>
#include <concepts>
#include <span>

template <typename T>
concept DSPStageBase = requires(T t) {
  { T::static_input_size() } -> std::convertible_to<size_t>;
  { T::static_output_size() } -> std::convertible_to<size_t>;
};

template <typename T, typename SampleT>
concept ImagStage =
    DSPStageBase<T> &&
    requires(T t, std::span<const SampleT> I_in, std::span<const SampleT> Q_in,
             std::span<SampleT> I_out, std::span<SampleT> Q_out) {
      { t.process(I_in, Q_in, I_out, Q_out) } -> std::same_as<void>;
    };

template <typename T, typename SampleT>
concept RealStage =
    DSPStageBase<T> &&
    requires(T t, std::span<const SampleT> I_in, std::span<const SampleT> Q_in,
             std::span<SampleT> out) {
      { t.process(I_in, Q_in, out) } -> std::same_as<void>;
    };

template <typename T, typename SampleT>
concept DSPStage = ImagStage<T, SampleT> || RealStage<T, SampleT>;

template <typename SampleT, typename... Stages>
consteval bool valid_pipeline() {
  constexpr std::array<bool, sizeof...(Stages)> stages{
      RealStage<Stages, SampleT>...};

  for (size_t i = 0; i + 1 < stages.size(); ++i)
    if (stages[i])
      return false;

  return true;
}

template <typename SampleT, typename... Stages>
concept ValidPipeline = valid_pipeline<SampleT, Stages...>();

template <typename SampleT, typename S1, typename S2>
constexpr bool compatible_sizes() {
  return S1::static_output_size() == S2::static_input_size();
}

template <typename SampleT, typename S1> constexpr bool valid_sizes() {
  return true;
}

template <typename SampleT, typename S1, typename S2, typename... Rest>
constexpr bool valid_sizes() {
  if constexpr (!compatible_sizes<SampleT, S1, S2>())
    return false;
  else
    return valid_sizes<SampleT, S2, Rest...>();
}

template <typename SampleT, typename... Stages>
concept ValidSizes = valid_sizes<SampleT, Stages...>();
