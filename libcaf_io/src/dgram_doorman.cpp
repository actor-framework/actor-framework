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

bool dgram_doorman::new_endpoint(execution_unit* ctx,
                                 dgram_scribe_handle endpoint,
                                 const void*, size_t num_bytes) {
  CAF_LOG_TRACE(CAF_ARG(endpoint) << CAF_ARG(num_bytes));
  if (detached())
    return false;
  using endpoint_t = new_endpoint_msg;
  using tmp_t = mailbox_element_vals<endpoint_t>;
  auto guard = parent_;
  auto& buf = rd_buf();
  CAF_ASSERT(buf.size() >= num_bytes);
  buf.resize(num_bytes);
  tmp_t tmp{strong_actor_ptr{}, message_id::make(),
            mailbox_element::forwarding_stack{},
            endpoint_t{hdl(), std::move(buf), endpoint,
                       parent()->local_port(hdl())}};
  invoke_mailbox_element_impl(ctx,tmp);
  return true;
}

} // namespace io
} // namespace caf
