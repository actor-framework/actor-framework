// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/abstract_channel.hpp"

#include "caf/actor_system.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {

abstract_channel::abstract_channel(int fs) : flags_(fs) {
  // nop
}

abstract_channel::~abstract_channel() {
  // nop
}

} // namespace caf
