// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <vector>

#include "caf/byte.hpp"

namespace caf {

/// A buffer for storing binary data.
using byte_buffer = std::vector<byte>;

} // namespace caf
