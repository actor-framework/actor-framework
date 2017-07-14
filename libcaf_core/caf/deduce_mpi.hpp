/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DEDUCE_MPI_HPP
#define CAF_DEDUCE_MPI_HPP

#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/param.hpp"
#include "caf/expected.hpp"
#include "caf/optional.hpp"
#include "caf/replies_to.hpp"
#include "caf/stream_result.hpp"

#include "caf/detail/implicit_conversions.hpp"

namespace caf {

namespace detail {

// dmi = deduce_mpi_implementation
template <class T>
struct dmi;

// case #1: function returning a single value
template <class Y, class... Xs>
struct dmi<Y (Xs...)> {
  using type = typed_mpi<type_list<typename param_decay<Xs>::type...>,
                         output_tuple<implicit_conversions_t<Y>>>;
};

// case #2a: function returning a result<...>
template <class... Ys, class... Xs>
struct dmi<result<Ys...> (Xs...)> {
  using type = typed_mpi<type_list<typename param_decay<Xs>::type...>,
                         output_tuple<implicit_conversions_t<Ys>...>>;
};

// case #2b: function returning a stream_result<...>
template <class Y, class... Xs>
struct dmi<stream_result<Y> (Xs...)> {
  using type = typed_mpi<type_list<typename param_decay<Xs>::type...>,
                         output_tuple<implicit_conversions_t<Y>>>;
};

// case #2c: function returning a std::tuple<...>
template <class... Ys, class... Xs>
struct dmi<std::tuple<Ys...> (Xs...)> {
  using type = typed_mpi<type_list<typename param_decay<Xs>::type...>,
                         output_tuple<implicit_conversions_t<Ys>...>>;
};

// case #2d: function returning a std::tuple<...>
template <class... Ys, class... Xs>
struct dmi<delegated<Ys...> (Xs...)> {
  using type = typed_mpi<type_list<typename param_decay<Xs>::type...>,
                         output_tuple<implicit_conversions_t<Ys>...>>;
};

// case #2d: function returning a typed_response_promise<...>
template <class... Ys, class... Xs>
struct dmi<typed_response_promise<Ys...> (Xs...)> {
  using type = typed_mpi<type_list<typename param_decay<Xs>::type...>,
                         output_tuple<implicit_conversions_t<Ys>...>>;
};

// case #3: function returning an optional<>
template <class Y, class... Xs>
struct dmi<optional<Y> (Xs...)> : dmi<Y (Xs...)> {};

// case #4: function returning an expected<>
template <class Y, class... Xs>
struct dmi<expected<Y> (Xs...)> : dmi<Y (Xs...)> {};

// case #5: function returning an annotated_stream<>
template <class Y, class... Ys, class... Xs>
struct dmi<annotated_stream<Y, Ys...> (Xs...)> : dmi<Y (Xs...)> {
  using type =
    typed_mpi<type_list<typename param_decay<Xs>::type...>,
              output_tuple<stream<Y>, strip_and_convert_t<Ys>...>>;
};

// -- dmfou = deduce_mpi_function_object_unboxing

template <class T, bool isClass = std::is_class<T>::value>
struct dmfou;

// case #1: const member function pointer
template <class C, class Result, class... Ts>
struct dmfou<Result (C::*)(Ts...) const, false> : dmi<Result (Ts...)> {};

// case #2: member function pointer
template <class C, class Result, class... Ts>
struct dmfou<Result (C::*)(Ts...), false> : dmi<Result (Ts...)> {};

// case #3: good ol' function
template <class Result, class... Ts>
struct dmfou<Result(Ts...), false> : dmi<Result (Ts...)> {};

template <class T>
struct dmfou<T, true> : dmfou<decltype(&T::operator()), false> {};

// this specialization leaves timeout definitions untouched,
// later stages such as interface_mismatch need to deal with them later
template <class T>
struct dmfou<timeout_definition<T>, true> {
  using type = timeout_definition<T>;
};

template <class T>
struct dmfou<trivial_match_case<T>, true> : dmfou<T> {};

} // namespace detail

/// Deduces the message passing interface from a function object.
template <class T>
using deduce_mpi_t = typename detail::dmfou<typename param_decay<T>::type>::type;

} // namespace caf

#endif // CAF_DEDUCE_MPI_HPP
