// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/doorman.hpp"

#include "caf/io/abstract_broker.hpp"

#include "caf/logger.hpp"

namespace caf::io {

doorman::doorman(accept_handle acc_hdl) : doorman_base(acc_hdl) {
  // nop
}

doorman::~doorman() {
  // nop
}

message doorman::detach_message() {
  return make_message(acceptor_closed_msg{hdl()});
}

bool doorman::new_connection(scheduler* ctx, connection_handle x) {
  msg().handle = x;
  return invoke_mailbox_element(ctx);
}

} // namespace caf::io
