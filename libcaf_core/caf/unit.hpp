// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"

#include <string>

namespace caf {

/// Unit is analogous to `void`, but can be safely returned, stored, etc.
/// to enable higher-order abstraction without cluttering code with
/// exceptions for `void` (which can't be stored, for example).
struct unit_t : detail::comparable<unit_t> {
  constexpr unit_t() noexcept = default;

  constexpr unit_t(const unit_t&) noexcept = default;

  constexpr unit_t& operator=(const unit_t&) noexcept = default;

  template <class T,
            class = std::enable_if_t<!std::is_same_v<std::decay_t<T>, unit_t>>>
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
