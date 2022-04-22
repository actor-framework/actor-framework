// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/message_oriented.hpp"

namespace caf::net {

message_oriented::upper_layer::~upper_layer() {
  // nop
}

message_oriented::lower_layer::~lower_layer() {
  // nop
}

} // namespace caf::net
