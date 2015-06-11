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

#ifndef CAF_EITHER_HPP
#define CAF_EITHER_HPP

#include <tuple>
#include <string>
#include <type_traits>

#include "caf/message.hpp"
#include "caf/replies_to.hpp"
#include "caf/type_name_access.hpp"
#include "caf/illegal_message_element.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {

/// @cond PRIVATE
std::string either_or_else_type_name(size_t lefts_size,
                                     const std::string* lefts,
                                     size_t rights_size,
                                     const std::string* rights);
template <class L, class R>
struct either_or_t;
template <class... Ls, class... Rs>
struct either_or_t<detail::type_list<Ls...>,
                   detail::type_list<Rs...>> : illegal_message_element {
  static_assert(! std::is_same<detail::type_list<Ls...>,
                              detail::type_list<Rs...>>::value,
                "template parameter packs must differ");
  either_or_t(Ls... ls) : value(make_message(std::move(ls)...)) {
    // nop
  }
  either_or_t(Rs... rs) : value(make_message(std::move(rs)...)) {
    // nop
  }
  using opt1_type = std::tuple<Ls...>;
  using opt2_type = std::tuple<Rs...>;
  message value;
  static std::string static_type_name() {
    std::string lefts[] = {type_name_access<Ls>()...};
    std::string rights[] = {type_name_access<Rs>()...};
    return either_or_else_type_name(sizeof...(Ls), lefts,
                                    sizeof...(Rs), rights);
  }
};
/// @endcond

template <class... Ts>
struct either {
  template <class... Us>
  using or_else = either_or_t<detail::type_list<Ts...>,
                              detail::type_list<Us...>>;
};

} // namespace caf

#endif // CAF_EITHER_HPP
