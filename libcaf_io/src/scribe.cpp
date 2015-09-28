/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/io/scribe.hpp"

#include "caf/detail/logging.hpp"

namespace caf {
namespace io {

scribe::scribe(abstract_broker* ptr, connection_handle conn_hdl)
    : scribe_base(ptr, conn_hdl) {
  // nop
}

scribe::~scribe() {
  CAF_LOG_TRACE("");
}

message scribe::detach_message() {
  return make_message(connection_closed_msg{hdl()});
}

void scribe::consume(const void*, size_t num_bytes) {
  CAF_LOG_TRACE(CAF_ARG(num_bytes));
  if (detached()) {
    // we are already disconnected from the broker while the multiplexer
    // did not yet remove the socket, this can happen if an IO event causes
    // the broker to call close_all() while the pollset contained
    // further activities for the broker
    return;
  }
  auto& buf = rd_buf();
  CAF_ASSERT(buf.size() >= num_bytes);
  // make sure size is correct, swap into message, and then call client
  buf.resize(num_bytes);
  auto& msg_buf = msg().buf;
  msg_buf.swap(buf);
  invoke_mailbox_element();
  // `mailbox_elem_ptr_ == nullptr` if the broker moved it to the cache
  if (mailbox_elem_ptr_) {
    // swap buffer back to stream and implicitly flush wr_buf()
    msg_buf.swap(buf);
    flush();
  }
}

void scribe::io_failure(network::operation op) {
  CAF_LOG_TRACE("id = " << hdl().id()
                << ", " << CAF_TARG(op, static_cast<int>));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  detach(true);
}

} // namespace io
} // namespace caf
