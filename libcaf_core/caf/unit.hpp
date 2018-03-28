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

#include "caf/detail/comparable.hpp"

namespace caf {

/// Unit is analogous to `void`, but can be safely returned, stored, etc.
/// to enable higher-order abstraction without cluttering code with
/// exceptions for `void` (which can't be stored, for example).
struct unit_t : detail::comparable<unit_t> {
  constexpr unit_t() noexcept {
    // nop
  }

  constexpr unit_t(const unit_t&) noexcept {
    // nop
  }

  template <class T>
  explicit constexpr unit_t(T&&) noexcept {
    // nop
  }

  static constexpr int compare(const unit_t&) noexcept {
    return 0;
  }

  template <class... Ts>
  constexpr unit_t operator()(Ts&&...) const noexcept {
    return {};
  }
};

static constexpr unit_t unit = unit_t{};

/// @relates unit_t
template <class Processor>
void serialize(Processor&, const unit_t&, unsigned int) {
  // nop
}

/// @relates unit_t
inline std::string to_string(const unit_t&) {
  return "unit";
}

template <class T>
struct lift_void {
  using type = T;
};

template <>
struct lift_void<void> {
  using type = unit_t;
};

template <class T>
struct unlift_void {
  using type = T;
};

template <>
struct unlift_void<unit_t> {
  using type = void;
};

} // namespace caf

