// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

namespace caf::io::network {

/// Identifies network IO operations, i.e., read or write.
enum class operation {
  read,
  write,
  propagate_error,
};

} // namespace caf::io::network
