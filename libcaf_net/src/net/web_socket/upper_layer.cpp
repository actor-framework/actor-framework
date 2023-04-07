// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/upper_layer.hpp"

namespace caf::net::web_socket {

upper_layer::~upper_layer() {
  // nop
}

upper_layer::server::~server() {
  // nop
}

} // namespace caf::net::web_socket
