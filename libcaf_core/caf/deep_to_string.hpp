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
#include <tuple>

#include "caf/detail/stringification_inspector.hpp"

namespace caf {

/// Unrolls collections such as vectors/maps, decomposes
/// tuples/pairs/arrays, auto-escapes strings and calls
/// `to_string` for user-defined types via argument-dependent
/// loopkup (ADL). Any user-defined type that does not
/// provide a `to_string` is mapped to `<unprintable>`.
template <class... Ts>
std::string deep_to_string(const Ts&... xs) {
  std::string result;
  detail::stringification_inspector f{result};
  f(xs...);
  return result;
}

/// Wrapper to `deep_to_string` for using the function as an inspector.
struct deep_to_string_t {
  using result_type = std::string;

  static constexpr bool reads_state = true;

  static constexpr bool writes_state = false;

  template <class... Ts>
  result_type operator()(const Ts&... xs) const {
    return deep_to_string(xs...);
  }
};

/// Convenience function for `deep_to_string(std::forward_as_tuple(xs...))`.
template <class... Ts>
std::string deep_to_string_as_tuple(const Ts&... xs) {
  return deep_to_string(std::forward_as_tuple(xs...));
}

} // namespace caf
