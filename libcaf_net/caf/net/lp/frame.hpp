// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/chunk.hpp"

namespace caf::net::lp {

/// An implicitly shared type for binary data frames.
// TODO: deprecate with 1.0
using frame = caf::chunk;

} // namespace caf::net::lp
