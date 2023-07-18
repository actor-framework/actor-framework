// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/span.hpp"

#include <cstddef>

namespace caf {

/// Convenience alias for referring to a writable sequence of bytes.
using byte_span = span<std::byte>;

/// Convenience alias for referring to a read-only sequence of bytes.
using const_byte_span = span<const std::byte>;

} // namespace caf
