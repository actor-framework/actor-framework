// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/datagram_servant.hpp"

#include "caf/logger.hpp"

namespace caf::io {

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
                                     size_t written, byte_buffer buffer) {
  CAF_LOG_TRACE(CAF_ARG(written));
  if (detached())
    return;
  using sent_t = datagram_sent_msg;
  mailbox_element tmp{strong_actor_ptr{}, make_message_id(),
                      make_message(sent_t{hdl, written, std::move(buffer)})};
  invoke_mailbox_element_impl(ctx, tmp);
}

} // namespace caf::io
