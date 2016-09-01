/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_REPLIES_TO_HPP
#define CAF_REPLIES_TO_HPP

#include <string>

#include "caf/illegal_message_element.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_pair.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// @cond PRIVATE
std::string replies_to_type_name(size_t input_size,
                                 const std::string* input,
                                 size_t output_opt1_size,
                                 const std::string* output_opt1);
/// @endcond

template <class...>
struct output_stream {};

template <class...>
struct output_tuple {};

template <class Input, class Output>
struct typed_mpi {};

/*
<detail::type_list<Is...>,
                 detail::type_list<Ls...>> {
  static_assert(sizeof...(Is) > 0, "template parameter pack Is empty");
  static_assert(sizeof...(Ls) > 0, "template parameter pack Ls empty");
  using input = detail::type_list<Is...>;
  using output = detail::type_list<Ls...>;
  static_assert(!detail::tl_exists<
                  input_types,
                  is_illegal_message_element
                >::value
                && !detail::tl_exists<
                  output_types,
                  is_illegal_message_element
                >::value,
                "interface definition contains an illegal message type");
};
*/

template <class... Is>
struct replies_to {
  template <class... Os>
  using with = typed_mpi<detail::type_list<Is...>, output_tuple<Os...>>;

  /// @private
  template <class... Os>
  using with_stream = typed_mpi<detail::type_list<Is...>, output_stream<Os...>>;
};

template <class... Is>
using reacts_to = typed_mpi<detail::type_list<Is...>, output_tuple<void>>;

} // namespace caf

#endif // CAF_REPLIES_TO_HPP
