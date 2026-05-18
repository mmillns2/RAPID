#pragma once

#include "dsp-controller-traits.h"
#include "frontend-traits.h"
#include <cstdint>
#include <type_traits>

template <typename T>
concept DAQStage = requires(T t) {
  { T::get_static_prescale() } -> std::convertible_to<uint32_t>;
  { T::static_accumulator_size() } -> std::convertible_to<uint32_t>;
  { T::static_accumulator_norm() } -> std::convertible_to<bool>;
};

template <typename SampleT, typename Stage> struct ComplexDAQStage {
private:
  static constexpr bool stage_is_complex() {
    if constexpr (FEDig<Stage, SampleT>)
      return ImagDig<Stage, SampleT>;
    else if constexpr (is_DSPController_v<Stage>)
      return is_ImagDSPController_v<Stage>;
    else if constexpr (DSPStage<Stage, SampleT>)
      return ImagStage<Stage, SampleT>;
    else
      return false;
  }

public:
  static constexpr bool value = stage_is_complex();
};

template <typename SampleT, typename Stage>
static constexpr bool ComplexDAQStage_v =
    ComplexDAQStage<SampleT, Stage>::value;
