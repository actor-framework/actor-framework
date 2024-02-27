// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/scribe.hpp"

#include "caf/log/io.hpp"

namespace caf::io {

scribe::scribe(connection_handle conn_hdl) : scribe_base(conn_hdl) {
  // nop
}

scribe::~scribe() {
  auto exit_guard = log::io::trace("");
}

message scribe::detach_message() {
  return make_message(connection_closed_msg{hdl()});
}

bool scribe::consume(execution_unit* ctx, const void*, size_t num_bytes) {
  CAF_ASSERT(ctx != nullptr);
  auto exit_guard = log::io::trace("num_bytes = {}", num_bytes);
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
  flush();
  return result;
}

void scribe::data_transferred(execution_unit* ctx, size_t written,
                              size_t remaining) {
  auto exit_guard = log::io::trace("written = {}, remaining = {}", written,
                                   remaining);
  if (detached())
    return;
  using transferred_t = data_transferred_msg;
  mailbox_element tmp{strong_actor_ptr{}, make_message_id(),
                      make_message(transferred_t{hdl(), written, remaining})};
  invoke_mailbox_element_impl(ctx, tmp);
  // data_transferred_msg tmp{hdl(), written, remaining};
  // auto ptr = make_mailbox_element(nullptr, invalid_message_id, {}, tmp);
  // parent()->context(ctx);
  // parent()->consume(std::move(ptr));
}

} // namespace caf::io
