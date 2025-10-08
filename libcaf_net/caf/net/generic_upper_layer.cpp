// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/generic_upper_layer.hpp"

#include "caf/log/net.hpp"

namespace caf::net {

generic_upper_layer::~generic_upper_layer() {
  // nop
}

void generic_upper_layer::handle_custom_event(uint8_t opcode,
                                              uint64_t payload) {
  caf::log::net::error("unhandled custom event: {}, {}", opcode, payload);
}

} // namespace caf::net
