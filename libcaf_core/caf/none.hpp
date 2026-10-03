// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <string>

namespace caf {

/// Represents "nothing", e.g., for clearing an `optional` by assigning `none`.
struct none_t {
  constexpr none_t() {
    // nop
  }

  constexpr explicit operator bool() const noexcept {
    return false;
  }

  constexpr auto operator<=>(const none_t&) const noexcept = default;
};

static constexpr none_t none = none_t{};

/// @relates none_t
inline std::string to_string(const none_t&) {
  return "none";
}

} // namespace caf
