/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/fwd.hpp"
#include "caf/replies_to.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {

template <class T>
struct output_types_of {
  // nop
};

template <class In, class Out>
struct output_types_of<typed_mpi<In, Out>> {
  using type = Out;
};

template <class T>
struct signatures_of {
  using type = typename std::remove_pointer<T>::type::signatures;
};

template <class T>
using signatures_of_t = typename signatures_of<T>::type;

template <class T>
constexpr bool statically_typed() {
  return !std::is_same<
           none_t,
           typename std::remove_pointer<T>::type::signatures
         >::value;
}

template <class T>
struct is_void_response : std::false_type {};

template <>
struct is_void_response<detail::type_list<void>> : std::true_type {};

// true for the purpose of type checking performed by send()
template <>
struct is_void_response<none_t> : std::true_type {};

} // namespace caf

