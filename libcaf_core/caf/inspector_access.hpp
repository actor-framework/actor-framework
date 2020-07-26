/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include "caf/detail/inspect.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Customization point for adding support for a custom type. The default
/// implementation requires an `inspect` overload for `T` that is available via
/// ADL.
template <class T>
struct inspector_access {
  template <class Inspector>
  static bool apply(Inspector& f, T& x) {
    using detail::inspect;
    using result_type = decltype(inspect(f, x));
    if constexpr (std::is_same<result_type, bool>::value)
      return inspect(f, x);
    else
      return apply_deprecated(f, x);
  }

  template <class Inspector>
  [[deprecated("inspect() overloads should return bool")]] static bool
  apply_deprecated(Inspector& f, T& x) {
    if (auto err = inspect(f, x)) {
      f.set_error(std::move(err));
      return false;
    }
    return true;
  }
};

/// Customization point to influence how inspectors treat fields of type `T`.
template <class T>
struct inspector_access_traits {
  static constexpr bool is_optional = false;
  static constexpr bool is_sum_type = false;
};

template <class T>
struct inspector_access_traits<optional<T>> {
  static constexpr bool is_optional = true;
  static constexpr bool is_sum_type = false;
};

template <class... Ts>
struct inspector_access_traits<variant<Ts...>> {
  static constexpr bool is_optional = false;
  static constexpr bool is_sum_type = true;
};

} // namespace caf
