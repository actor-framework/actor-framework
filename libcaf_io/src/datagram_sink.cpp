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

#include "caf/io/datagram_sink.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace io {

datagram_sink::datagram_sink(abstract_broker* parent, datagram_sink_handle hdl)
    : datagram_sink_base(parent, hdl) {
  // nop
}

datagram_sink::~datagram_sink() {
  CAF_LOG_TRACE("");
}

message datagram_sink::detach_message() {
  return make_message(datagram_sink_closed_msg{hdl()});
}

void datagram_sink::datagram_sent(execution_unit* ctx, size_t written) {
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

void datagram_sink::io_failure(execution_unit* ctx, network::operation op) {
  CAF_LOG_TRACE(CAF_ARG(hdl()) << CAF_ARG(op));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  detach(ctx, true);
}

} // namespace io
} // namespace caf
