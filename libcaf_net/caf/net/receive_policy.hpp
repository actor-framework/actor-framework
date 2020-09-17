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

#include <cstdint>

namespace caf::net {

struct receive_policy {
  uint32_t min_size;
  uint32_t max_size;

  /// @pre `min_size > 0`
  /// @pre `min_size <= max_size`
  static constexpr receive_policy between(uint32_t min_size,
                                          uint32_t max_size) {
    return {min_size, max_size};
  }

  /// @pre `size > 0`
  static constexpr receive_policy exactly(uint32_t size) {
    return {size, size};
  }

  static constexpr receive_policy up_to(uint32_t max_size) {
    return {1, max_size};
  }

  static constexpr receive_policy stop() {
    return {0, 0};
  }
};

} // namespace caf::net
