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

#include "caf/io/dgram_scribe.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace io {

dgram_scribe::dgram_scribe(abstract_broker* parent, dgram_scribe_handle hdl)
    : dgram_scribe_base(parent, hdl) {
  // nop
}

dgram_scribe::~dgram_scribe() {
  CAF_LOG_TRACE("");
}

message dgram_scribe::detach_message() {
  return make_message(dgram_scribe_closed_msg{hdl()});
}

bool dgram_scribe::consume(execution_unit* ctx, const void*,
                                 size_t num_bytes) {
  CAF_ASSERT(ctx != nullptr);
  CAF_LOG_TRACE(CAF_ARG(num_bytes));
  if (detached())
    // we are already disconnected from the broker while the multiplexer
    // did not yet remove the socket, this can happen if an I/O event causes
    // the broker to call close_all() while the pollset contained
    // further activities for the broker
    return false;
  // keep a strong reference to our parent until we leave scope
  // to avoid UB when becoming detached during invocation
  auto guard = parent_;
  auto& buf = rd_buf();
  CAF_ASSERT(buf.size() >= num_bytes);
  // make sure size is correct, swap into message, and then call client
  buf.resize(num_bytes);
  auto& msg_buf = msg().buf;
  msg_buf.swap(buf);
  auto result = invoke_mailbox_element(ctx);
  // swap buffer back to stream and implicitly flush wr_buf()
  msg_buf.swap(buf);
  // flush(); // <-- TODO: here from scribe, not sure why?
  return result;
}

void dgram_scribe::datagram_sent(execution_unit* ctx, size_t written) {
  CAF_LOG_TRACE(CAF_ARG(written));
  if (detached())
    return;
  using sent_t = datagram_sent_msg;
  using tmp_t = mailbox_element_vals<datagram_sent_msg>;
  tmp_t tmp{strong_actor_ptr{}, message_id::make(),
            mailbox_element::forwarding_stack{},
            sent_t{hdl(), written}};
  invoke_mailbox_element_impl(ctx, tmp);
}

void dgram_scribe::io_failure(execution_unit* ctx, network::operation op) {
  CAF_LOG_TRACE(CAF_ARG(hdl()) << CAF_ARG(op));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  detach(ctx, true);
}

} // namespace io
} // namespace caf
