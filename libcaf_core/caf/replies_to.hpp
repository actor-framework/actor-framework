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

#ifndef CAF_REPLIES_TO_HPP
#define CAF_REPLIES_TO_HPP

#include <string>

#include "caf/either.hpp"
#include "caf/type_name_access.hpp"
#include "caf/uniform_type_info.hpp"
#include "caf/illegal_message_element.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_pair.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// @cond PRIVATE
std::string replies_to_type_name(size_t input_size,
                                 const std::string* input,
                                 size_t output_opt1_size,
                                 const std::string* output_opt1,
                                 size_t output_opt2_size,
                                 const std::string* output_opt2);
/// @endcond

template <class InputTypes, class LeftOutputTypes, class RightOutputTypes>
struct typed_mpi;

template <class... Is, class... Ls, class... Rs>
struct typed_mpi<detail::type_list<Is...>,
                 detail::type_list<Ls...>,
                 detail::type_list<Rs...>> {
  static_assert(sizeof...(Is) > 0, "template parameter pack Is empty");
  static_assert(sizeof...(Ls) > 0, "template parameter pack Ls empty");
  using input_types = detail::type_list<Is...>;
  using output_opt1_types = detail::type_list<Ls...>;
  using output_opt2_types = detail::type_list<Rs...>;
  static_assert(! std::is_same<output_opt1_types, output_opt2_types>::value,
                "result types must differ when using with_either<>::or_else<>");
  static_assert(! detail::tl_exists<
                  input_types,
                  is_illegal_message_element
                >::value
                && ! detail::tl_exists<
                  output_opt1_types,
                  is_illegal_message_element
                >::value
                && ! detail::tl_exists<
                  output_opt2_types,
                  is_illegal_message_element
                >::value,
                "interface definition contains an illegal message type, "
                "did you use with<either...> instead of with_either<...>?");
  static std::string static_type_name() {
    std::string input[] = {type_name_access<Is>()...};
    std::string output_opt1[] = {type_name_access<Ls>()...};
    // Rs... is allowed to be empty, hence we need to add a dummy element
    // to make sure this array is not of size 0 (to prevent compiler errors)
    std::string output_opt2[] = {std::string(), type_name_access<Rs>()...};
    return replies_to_type_name(sizeof...(Is), input,
                                sizeof...(Ls), output_opt1,
                                sizeof...(Rs), output_opt2 + 1);
  }
};

template <class... Is>
struct replies_to {
  template <class... Os>
  using with = typed_mpi<detail::type_list<Is...>,
                         detail::type_list<Os...>,
                         detail::empty_type_list>;
  template <class... Ls>
  struct with_either {
    template <class... Rs>
    using or_else = typed_mpi<detail::type_list<Is...>,
                              detail::type_list<Ls...>,
                              detail::type_list<Rs...>>;
  };
};

template <class... Is>
using reacts_to = typed_mpi<detail::type_list<Is...>,
                            detail::type_list<void>,
                            detail::empty_type_list>;

} // namespace caf

#endif // CAF_REPLIES_TO_HPP
