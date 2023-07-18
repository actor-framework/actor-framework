// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"

#include <string>

namespace caf {

/// Represents "nothing", e.g., for clearing an `optional` by assigning `none`.
struct none_t : detail::comparable<none_t> {
  constexpr none_t() {
    // nop
  }
  constexpr explicit operator bool() const {
    return false;
  }

  static constexpr int compare(none_t) {
    return 0;
  }
};

static constexpr none_t none = none_t{};

/// @relates none_t
inline std::string to_string(const none_t&) {
  return "none";
}

} // namespace caf
