// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/connection_guard.hpp"

namespace caf::detail {

connection_guard::~connection_guard() {
  // nop
}

} // namespace caf::detail
