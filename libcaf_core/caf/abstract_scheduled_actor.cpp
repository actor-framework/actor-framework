// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/abstract_scheduled_actor.hpp"

#include "caf/message_id.hpp"
#include "caf/message_priority.hpp"

namespace caf {

abstract_scheduled_actor::~abstract_scheduled_actor() {
  // nop
}

message_id
abstract_scheduled_actor::new_request_id(message_priority mp) noexcept {
  auto result = ++last_request_id_;
  return mp == message_priority::normal ? result : result.with_high_priority();
}

uint64_t abstract_scheduled_actor::new_u64_id() noexcept {
  auto result = ++last_request_id_;
  return result.integer_value();
}

} // namespace caf
