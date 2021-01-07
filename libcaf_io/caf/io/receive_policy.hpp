// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <string>
#include <utility>

#include "caf/config.hpp"

namespace caf::io {

enum class receive_policy_flag : unsigned { at_least, at_most, exactly };

constexpr unsigned to_integer(receive_policy_flag x) {
  return static_cast<unsigned>(x);
}

inline std::string to_string(receive_policy_flag x) {
  return x == receive_policy_flag::at_least
           ? "at_least"
           : (x == receive_policy_flag::at_most ? "at_most" : "exactly");
}

class receive_policy {
public:
  receive_policy() = delete;

  using config = std::pair<receive_policy_flag, size_t>;

  static config at_least(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::at_least, num_bytes};
  }

  static config at_most(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::at_most, num_bytes};
  }

  static config exactly(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::exactly, num_bytes};
  }
};

} // namespace caf::io
