// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/detail/implicit_conversions.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

// dmi = deduce_mpi_implementation
template <class T>
struct dmi;

// case #1: function returning a single value
template <class Out, class... In>
struct dmi<Out(In...)> {
  using type = result<implicit_conversions_t<Out>>(std::decay_t<In>...);
};

// case #2: function returning a result<...>
template <class... Out, class... In>
struct dmi<result<Out...>(In...)> {
  using type = result<Out...>(std::decay_t<In>...);
};

// case #3: function returning a delegated<...>
template <class... Out, class... In>
struct dmi<delegated<Out...>(In...)> {
  using type = result<Out...>(std::decay_t<In>...);
};

// case #4: function returning a typed_response_promise<...>
template <class... Out, class... In>
struct dmi<typed_response_promise<Out...>(In...)> {
  using type = result<Out...>(std::decay_t<In>...);
};

// -- dmfou = deduce_mpi_function_object_unboxing

template <class T, bool isClass = std::is_class<T>::value>
struct dmfou;

// case #1a: const member function pointer
template <class C, class Out, class... In>
struct dmfou<Out (C::*)(In...) const, false> : dmi<Out(In...)> {};

// case #1b: const member function pointer with noexcept
template <class C, class Out, class... In>
struct dmfou<Out (C::*)(In...) const noexcept, false> : dmi<Out(In...)> {};

// case #2a: member function pointer
template <class C, class Out, class... In>
struct dmfou<Out (C::*)(In...), false> : dmi<Out(In...)> {};

// case #2b: member function pointer with noexcept
template <class C, class Out, class... In>
struct dmfou<Out (C::*)(In...) noexcept, false> : dmi<Out(In...)> {};

// case #3a: good ol' function
template <class Out, class... In>
struct dmfou<Out(In...), false> : dmi<Out(In...)> {};

// case #3a: good ol' function with noexcept
template <class Out, class... In>
struct dmfou<Out(In...) noexcept, false> : dmi<Out(In...)> {};

template <class T>
struct dmfou<T, true> : dmfou<decltype(&T::operator()), false> {};

// this specialization leaves timeout definitions untouched,
// later stages such as interface_mismatch need to deal with them later
template <class T>
struct dmfou<timeout_definition<T>, true> {
  using type = timeout_definition<T>;
};

} // namespace caf::detail

namespace caf {

/// Deduces the message passing interface from a function object.
template <class T>
using deduce_mpi_t = typename detail::dmfou<std::decay_t<T>>::type;

} // namespace caf
