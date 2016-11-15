/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/dgram_doorman.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace io {

dgram_doorman::dgram_doorman(abstract_broker* parent, dgram_doorman_handle hdl)
    : dgram_doorman_base(parent, hdl) {
  // nop
}

dgram_doorman::~dgram_doorman() {
  CAF_LOG_TRACE("");
}

message dgram_doorman::detach_message() {
  return make_message(dgram_doorman_closed_msg{hdl()});
}

void dgram_doorman::io_failure(execution_unit* ctx, network::operation op) {
  CAF_LOG_TRACE(CAF_ARG(hdl()) << CAF_ARG(op));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  detach(ctx, true);
}

// bool dgram_doorman::new_endpoint(execution_unit* ctx, const void* buf,
//                                  size_t besize) {
bool dgram_doorman::new_endpoint(execution_unit* ctx,
                                 dgram_scribe_handle endpoint) {
  msg().handle = endpoint;
  return invoke_mailbox_element(ctx);
}

} // namespace io
} // namespace caf
