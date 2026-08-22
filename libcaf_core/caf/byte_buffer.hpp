// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <cstddef>
#include <vector>

namespace caf {

/// A buffer for storing binary data.
using byte_buffer = std::vector<std::byte>;

/// Converts a string to a byte buffer.
CAF_CORE_EXPORT byte_buffer to_byte_buffer(const char* str, size_t len);

/// Copies a sequence of bytes into a byte buffer.
CAF_CORE_EXPORT byte_buffer to_byte_buffer(const std::byte* data, size_t len);

} // namespace caf
