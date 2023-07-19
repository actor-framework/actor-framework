// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/message_handler.hpp"

#include "caf/config.hpp"

namespace caf {

message_handler::message_handler(impl_ptr ptr) : impl_(std::move(ptr)) {
  // nop
}

void message_handler::assign(message_handler what) {
  impl_.swap(what.impl_);
}

} // namespace caf
