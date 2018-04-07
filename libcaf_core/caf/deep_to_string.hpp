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

#include <array>
#include <tuple>
#include <string>
#include <utility>
#include <functional>
#include <type_traits>

#include "caf/atom.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/stringification_inspector.hpp"

namespace caf {

class deep_to_string_t {
public:
  constexpr deep_to_string_t() {
    // nop
  }

  template <class... Ts>
  std::string operator()(Ts&&... xs) const {
    std::string result;
    detail::stringification_inspector f{result};
    f(std::forward<Ts>(xs)...);
    return result;
  }
};

/// Unrolls collections such as vectors/maps, decomposes
/// tuples/pairs/arrays, auto-escapes strings and calls
/// `to_string` for user-defined types via argument-dependent
/// loopkup (ADL). Any user-defined type that does not
/// provide a `to_string` is mapped to `<unprintable>`.
constexpr deep_to_string_t deep_to_string = deep_to_string_t{};

/// Convenience function for `deep_to_string(std::forward_as_tuple(xs...))`.
template <class... Ts>
std::string deep_to_string_as_tuple(Ts&&... xs) {
  return deep_to_string(std::forward_as_tuple(std::forward<Ts>(xs)...));
}

} // namespace caf

