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
  enum class validation_result {
    valid = 0,
    incomplete_data,
    invalid_code_point,
    malformed_message,
  };

  // 1. suggestion

  /// Checks whether the begining of `bytes` is a valid UTF-8 string and returns
  /// the end of the validated segment.
  /// In case of validation_result::valid, the index is equal to bytes.size()
  /// Otherwise, index points to the end of the valid utf-8 range between [0,
  /// size) and the validation_result indicates the reason of failure.
  static std::pair<const_byte_span::index_type, validation_result>
  validate_prefix(const_byte_span bytes) noexcept;

  /// Returns -1 on hard failure, index otherwise
  static const_byte_span::index_type
  validate_prefix(const_byte_span bytes) noexcept;

  /// Returns validation result and sets begin to end of the valid utf-8 range
  static validation_result
  validate_prefix(const_byte_span::iterator& begin,
                  const_byte_span::iterator end) noexcept;

  /// Checks whether `str` is a valid UTF-8 string.
  static std::optional<std::string_view::size_type>
  validate_prefix(std::string_view str) noexcept;

  /// Checks whether `bytes` is a valid UTF-8 string.
  static bool valid(const_byte_span bytes) noexcept;

  /// Checks whether `str` is a valid UTF-8 string.
  static bool valid(std::string_view str) noexcept {
    return valid(as_bytes(make_span(str)));
  }
};

} // namespace caf::detail
