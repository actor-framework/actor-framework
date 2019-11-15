/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <type_traits>

#include "caf/config_value.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf::detail {

template <class Trait>
struct dispatch_parse_cli_helper {
  template <class... Ts>
  auto operator()(Ts&&... xs)
    -> decltype(Trait::parse_cli(std::forward<Ts>(xs)...)) {
    return Trait::parse_cli(std::forward<Ts>(xs)...);
  }
};

template <class Access, class T>
void dispatch_parse_cli(std::true_type, string_parser_state& ps, T& x,
                        const char* char_blacklist) {
  Access::parse_cli(ps, x, char_blacklist);
}

template <class Access, class T>
void dispatch_parse_cli(std::false_type, string_parser_state& ps, T& x,
                        const char*) {
  Access::parse_cli(ps, x);
}

template <class T>
void dispatch_parse_cli(string_parser_state& ps, T& x,
                        const char* char_blacklist) {
  using access = caf::select_config_value_access_t<T>;
  using helper_fun = dispatch_parse_cli_helper<access>;
  using token_type = bool_token<detail::is_callable_with<
    helper_fun, string_parser_state&, T&, const char*>::value>;
  token_type token;
  dispatch_parse_cli<access>(token, ps, x, char_blacklist);
}

} // namespace caf
