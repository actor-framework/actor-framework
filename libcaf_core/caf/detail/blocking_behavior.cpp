// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/blocking_behavior.hpp"

namespace caf::detail {

blocking_behavior::~blocking_behavior() {
  // nop
}

blocking_behavior::blocking_behavior(behavior& x) : nested(x) {
  // nop
}

skippable_result blocking_behavior::fallback(message&) {
  return skip;
}

timespan blocking_behavior::timeout() {
  return infinite;
}

void blocking_behavior::handle_timeout() {
  // nop
}

} // namespace caf::detail
