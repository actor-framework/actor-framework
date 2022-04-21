// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <vector>

namespace caf {

/// A buffer for storing binary data.
using byte_buffer = std::vector<std::byte>;

} // namespace caf
