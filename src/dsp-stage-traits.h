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
struct ValidPipeline : std::true_type {};

template <typename SampleT, typename S1>
struct ValidPipeline<SampleT, S1> : std::true_type {};

template <typename SampleT, typename S1, typename S2, typename... Rest>
struct ValidPipeline<SampleT, S1, S2, Rest...> {
  static constexpr bool value =
      !RealStage<S1, SampleT> && ValidPipeline<SampleT, S2, Rest...>::value;
};

template <typename SampleT, typename... Stages>
static constexpr bool ValidPipeline_v =
    ValidPipeline<SampleT, Stages...>::value;

template <typename S1, typename S2> struct CompatibleSizes {
  static constexpr bool value =
      S1::static_output_size() == S2::static_input_size();
};

template <typename S1, typename S2>
static constexpr bool CompatibleSizes_v = CompatibleSizes<S1, S2>::value;

template <typename... Stages> struct ValidSizes : std::true_type {};

template <typename S1, typename S2, typename... Rest>
struct ValidSizes<S1, S2, Rest...> {
  static constexpr bool value =
      CompatibleSizes_v<S1, S2> && ValidSizes<S2, Rest...>::value;
};

template <typename... Stages>
static constexpr bool ValidSizes_v = ValidSizes<Stages...>::value;
