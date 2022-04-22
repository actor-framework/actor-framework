// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/stream_oriented.hpp"

namespace caf::net {

stream_oriented::upper_layer::~upper_layer() {
  // nop
}

stream_oriented::lower_layer::~lower_layer() {
  // nop
}

} // namespace caf::net
