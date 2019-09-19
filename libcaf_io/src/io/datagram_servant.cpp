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

#include "caf/io/datagram_servant.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace io {

datagram_servant::datagram_servant(datagram_handle hdl)
  : datagram_servant_base(hdl) {
  // nop
}

datagram_servant::~datagram_servant() {
  // nop
}

message datagram_servant::detach_message() {
  return make_message(datagram_servant_closed_msg{hdls()});
}

bool datagram_servant::consume(execution_unit* ctx, datagram_handle hdl,
                               network::receive_buffer& buf) {
  CAF_ASSERT(ctx != nullptr);
  CAF_LOG_TRACE(CAF_ARG(buf.size()));
  if (detached()) {
    // we are already disconnected from the broker while the multiplexer
    // did not yet remove the socket, this can happen if an I/O event causes
    // the broker to call close_all() while the pollset contained
    // further activities for the broker
    return false;
  }
  // keep a strong reference to our parent until we leave scope
  // to avoid UB when becoming detached during invocation
  auto guard = parent_;
  msg().handle = hdl;
  auto& msg_buf = msg().buf;
  msg_buf.swap(buf);
  auto result = invoke_mailbox_element(ctx);
  // swap buffer back to stream and implicitly flush wr_buf()
  msg_buf.swap(buf);
  flush();
  return result;
}

void datagram_servant::datagram_sent(execution_unit* ctx, datagram_handle hdl,
                                     size_t written, std::vector<char> buffer) {
  CAF_LOG_TRACE(CAF_ARG(written));
  if (detached())
    return;
  using sent_t = datagram_sent_msg;
  using tmp_t = mailbox_element_vals<datagram_sent_msg>;
  tmp_t tmp{strong_actor_ptr{}, make_message_id(),
            mailbox_element::forwarding_stack{},
            sent_t{hdl, written, std::move(buffer)}};
  invoke_mailbox_element_impl(ctx, tmp);
}

} // namespace io
} // namespace caf

