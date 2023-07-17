// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/socket_event_layer.hpp"

namespace caf::net {

socket_event_layer::~socket_event_layer() {
  // nop
}

bool socket_event_layer::do_handover(std::unique_ptr<socket_event_layer>&) {
  return false;
}

bool socket_event_layer::finalized() const noexcept {
  return true;
}

} // namespace caf::net
