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
struct signatures_of {
  using type = typename std::remove_pointer<T>::type::signatures;
};

template <class T>
constexpr bool statically_typed() {
  return ! std::is_same<
           none_t,
           typename std::remove_pointer<T>::type::signatures
         >::value;
}

template <class Signatures, class Input>
struct actor_accepts_message;

template <class Input>
struct actor_accepts_message<none_t, Input> : std::true_type {};

template <class... Ts, class Input>
struct actor_accepts_message<detail::type_list<Ts...>, Input> 
    : detail::tl_exists<detail::type_list<Ts...>, 
                        detail::input_is<Input>::template eval> {};

template <class Signatures, class Input>
struct response_to;

template <class Input>
struct response_to<none_t, Input> {
  using type = none_t;
};

template <class... Ts, class Input>
struct response_to<detail::type_list<Ts...>, Input> {
  using type =
    typename output_types_of<
      typename detail::tl_find<
        detail::type_list<Ts...>,
        detail::input_is<Input>::template eval
      >::type
    >::type;
};

template <class T>
struct is_void_response : std::false_type {};

template <>
struct is_void_response<detail::type_list<void>> : std::true_type {};

// true for the purpose of type checking performed by send()
template <>
struct is_void_response<none_t> : std::true_type {};

} // namespace caf

#endif // CAF_CHECK_TYPED_INPUT_HPP
