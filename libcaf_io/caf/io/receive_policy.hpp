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
#include <cstddef>
#include <utility>

#include "caf/config.hpp"

namespace caf {
namespace io {

enum class receive_policy_flag : unsigned {
  at_least,
  at_most,
  exactly
};

inline std::string to_string(receive_policy_flag x) {
  return x == receive_policy_flag::at_least
         ? "at_least"
         : (x == receive_policy_flag::at_most ? "at_most" : "exactly");
}

class receive_policy {
public:
  receive_policy() = delete;

  using config = std::pair<receive_policy_flag, size_t>;

  static inline config at_least(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::at_least, num_bytes};
  }

  static inline config at_most(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::at_most, num_bytes};
  }

  static inline config exactly(size_t num_bytes) {
    CAF_ASSERT(num_bytes > 0);
    return {receive_policy_flag::exactly, num_bytes};
  }
};

} // namespace io
} // namespace caf

