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

#include <string>

#include "caf/illegal_message_element.hpp"
#include "caf/output_stream.hpp"
#include "caf/stream.hpp"

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
struct output_tuple {};

template <class Input, class Output>
struct typed_mpi {};

template <class... Is>
struct replies_to {
  template <class... Os>
  using with = typed_mpi<detail::type_list<Is...>, output_tuple<Os...>>;

  /// @private
  template <class O, class... Os>
  using with_stream = typed_mpi<detail::type_list<Is...>,
                                output_stream<O, Os...>>;
};

template <class... Is>
using reacts_to = typed_mpi<detail::type_list<Is...>, output_tuple<void>>;

} // namespace caf

