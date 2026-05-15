#pragma once

#include <tuple>

template <typename... Stages> struct StageTuple {
  using type = std::tuple<Stages...>;
};

template <typename... Stages>
using StageTuple_t = typename StageTuple<Stages...>::type;

template <std::size_t I, typename... Stages> struct StageTupleElement {
  using type = std::tuple_element_t<I, StageTuple_t<Stages...>>;
};

template <std::size_t I, typename... Stages>
using StageElement_t = typename StageTupleElement<I, Stages...>::type;
