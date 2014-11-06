/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_WILDCARD_POSITION_HPP
#define CAF_WILDCARD_POSITION_HPP

#include <type_traits>

#include "caf/anything.hpp"
#include "caf/detail/type_list.hpp"

namespace caf {

/**
 * Denotes the position of `anything` in a template parameter pack.
 */
enum class wildcard_position {
  nil,
  trailing,
  leading,
  in_between,
  multiple
};

/**
 * Gets the position of `anything` from the type list `Types`.
 */
template <class Types>
constexpr wildcard_position get_wildcard_position() {
  return detail::tl_count<Types, is_anything>::value > 1
         ? wildcard_position::multiple
         : (detail::tl_count<Types, is_anything>::value == 1
            ? (std::is_same<
                 typename detail::tl_head<Types>::type, anything
               >::value
               ? wildcard_position::leading
               : (std::is_same<
                    typename detail::tl_back<Types>::type, anything
                  >::value
                  ? wildcard_position::trailing
                  : wildcard_position::in_between))
            : wildcard_position::nil);
}

} // namespace caf

#endif // CAF_WILDCARD_POSITION_HPP
