/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/doorman.hpp"

#include "caf/logger.hpp"

#include "caf/io/abstract_broker.hpp"

namespace caf {
namespace io {

doorman::doorman(accept_handle acc_hdl) : doorman_base(acc_hdl) {
  // nop
}

doorman::~doorman() {
  // nop
}

message doorman::detach_message() {
  return make_message(acceptor_closed_msg{hdl()});
}

bool doorman::new_connection(execution_unit* ctx, connection_handle x) {
  msg().handle = x;
  return invoke_mailbox_element(ctx);
}

} // namespace io
} // namespace caf
