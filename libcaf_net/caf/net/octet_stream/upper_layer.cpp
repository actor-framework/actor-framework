// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/octet_stream/upper_layer.hpp"

namespace caf::net::octet_stream {

upper_layer::~upper_layer() {
  // nop
}

void upper_layer::written(size_t) {
  // nop
}

} // namespace caf::net::octet_stream
