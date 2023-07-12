// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/core_export.hpp"

#include <cstddef>

namespace caf::detail {

/// Wraps functions for processing RFC 3629 encoding, i.e., UTF-8. See
/// https://datatracker.ietf.org/doc/html/rfc3629 for details.
class CAF_CORE_EXPORT rfc3629 {
public:
  /// Checks whether `bytes` is a valid UTF-8 string.
  static bool valid(const_byte_span bytes) noexcept;

  /// Checks whether `str` is a valid UTF-8 string.
  static bool valid(std::string_view str) noexcept {
    return valid(as_bytes(make_span(str)));
  }

  /// Checks whether `bytes` is a valid UTF-8 string.
  static auto validate(const_byte_span bytes) noexcept
    -> std::pair<size_t, bool>;

  /// Checks whether `str` is a valid UTF-8 string.
  static auto validate(std::string_view str) noexcept
    -> std::pair<size_t, bool> {
    return validate(as_bytes(make_span(str)));
  }
};

} // namespace caf::detail
