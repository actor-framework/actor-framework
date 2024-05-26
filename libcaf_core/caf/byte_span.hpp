// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/span.hpp"

#include <cstddef>

namespace caf {

/// Convenience alias for referring to a writable sequence of bytes.
using byte_span = span<std::byte>;

/// Convenience alias for referring to a read-only sequence of bytes.
using const_byte_span = span<const std::byte>;

/// Checks whether the byte span is a valid UTF-8 string.
CAF_CORE_EXPORT bool is_valid_utf8(const_byte_span) noexcept;

/// Reinterprets the underlying data as a string view.
CAF_CORE_EXPORT std::string_view to_string_view(const_byte_span) noexcept;

} // namespace caf
