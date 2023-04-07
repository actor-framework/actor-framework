// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>

namespace caf::net {

/// Configures how many bytes an octet stream transport receives before calling
/// `consume` on its upper layer.
struct receive_policy {
  /// Configures how many bytes the transport  must read before it may call
  /// `consume` on its upper layer.
  uint32_t min_size;

  /// Configures how many bytes the transport may read at most before it calls
  /// `consume` on its upper layer.
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

  /// @pre `max_size >= 1`
  static constexpr receive_policy up_to(uint32_t max_size) {
    return {1, max_size};
  }

  static constexpr receive_policy stop() {
    return {0, 0};
  }
};

} // namespace caf::net
