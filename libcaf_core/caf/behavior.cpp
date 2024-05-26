// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/behavior.hpp"

#include "caf/message_handler.hpp"
#include "caf/none.hpp"

namespace caf {

behavior::behavior(const message_handler& mh) : impl_(mh.as_behavior_impl()) {
  // nop
}

void behavior::assign(message_handler other) {
  impl_.swap(other.impl_);
}

void behavior::assign(behavior other) {
  impl_.swap(other.impl_);
}

} // namespace caf
