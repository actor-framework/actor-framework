/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_CHECK_TYPED_INPUT_HPP
#define CAF_CHECK_TYPED_INPUT_HPP

#include "caf/fwd.hpp"
#include "caf/typed_actor.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/typed_actor_util.hpp"

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
constexpr typename std::remove_pointer<T>::type::signatures signatures_of() {
  return {};
}

template <class T>
constexpr bool statically_typed() {
  return ! std::is_same<
           none_t,
           typename std::remove_pointer<T>::type::signatures
         >::value;
}

template <class Signatures, class MessageTypes>
constexpr bool actor_accepts_message(Signatures, MessageTypes) {
  return detail::tl_exists<
           Signatures,
           detail::input_is<MessageTypes>::template eval
         >::value;
}

template <class MessageTypes>
constexpr bool actor_accepts_message(none_t, MessageTypes) {
  return true;
}

template <class Signatures, class MessageTypes>
constexpr typename output_types_of<
  typename detail::tl_find<
    Signatures,
    detail::input_is<MessageTypes>::template eval
  >::type
>::type
response_to(Signatures, MessageTypes) {
  return {};
}

template <class MessageTypes>
constexpr none_t response_to(none_t, MessageTypes) {
  return {};
}

template <class... Ts>
constexpr bool is_void_response(detail::type_list<Ts...>) {
  return false;
}

constexpr bool is_void_response(detail::type_list<void>) {
  return true;
}

constexpr bool is_void_response(none_t) {
  return true; // true for the purpose of type checking performed by send()
}

} // namespace caf

#endif // CAF_CHECK_TYPED_INPUT_HPP
