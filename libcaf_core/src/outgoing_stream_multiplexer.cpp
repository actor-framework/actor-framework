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

#include "caf/detail/outgoing_stream_multiplexer.hpp"

#include "caf/send.hpp"
#include "caf/variant.hpp"
#include "caf/to_string.hpp"
#include "caf/local_actor.hpp"

namespace caf {
namespace detail {

outgoing_stream_multiplexer::outgoing_stream_multiplexer(local_actor* self,
                                                         backend& service)
    : stream_multiplexer(self, service) {
  // nop
}

void outgoing_stream_multiplexer::operator()(stream_msg& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(self_->current_mailbox_element() != nullptr);
  dispatch(*this, x);
}

void outgoing_stream_multiplexer::operator()(stream_msg::open& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(current_stream_msg_ != nullptr);
  auto predecessor = std::move(x.prev_stage);
  // Make sure we a previous stage.
  if (!predecessor) {
    CAF_LOG_WARNING("received stream_msg::open without previous stage");
    return fail(sec::invalid_upstream, nullptr);
  }
  // Make sure we don't receive a handshake for an already open stream.
  if (streams_.count(current_stream_msg_->sid) > 0) {
    CAF_LOG_WARNING("received stream_msg::open twice");
    return fail(sec::upstream_already_exists, std::move(predecessor));
  }
  // Make sure we have a next stage.
  auto cme = self_->current_mailbox_element();
  if (cme->stages.empty()) {
    CAF_LOG_WARNING("received stream_msg::open without next stage");
    return fail(sec::invalid_downstream, std::move(predecessor));
  }
  auto successor = cme->stages.back();
  // Get a connection to the responsible stream server.
  auto path = get_remote_or_try_connect(successor->node());
  if (!path) {
    CAF_LOG_WARNING("cannot connect to remote stream server");
    return fail(sec::cannot_connect_to_node, std::move(predecessor));
  }
  // Update state and send handshake to remote stream_serv (via
  // middleman/basp_broker).
  streams_.emplace(current_stream_msg_->sid,
                   stream_state{std::move(predecessor), path->hdl,
                   &(*path)});
  // Send handshake to remote stream_serv (via middleman/basp_broker). We need
  // to send this message as `current_sender`. We have do bypass the queue
  // since `send_remote` always sends the message from `self_`.
  auto ptr = make_mailbox_element(
    cme->sender, message_id::make(), {}, forward_atom::value, cme->sender,
    std::move(cme->stages), path->hdl, cme->mid,
    make_message(make<stream_msg::open>(current_stream_msg_->sid,
                                        std::move(x.token), self_->ctrl(),
                                        x.priority, std::move(x.topics),
                                        x.redeployable)));
  basp()->enqueue(std::move(ptr), self_->context());
}

void outgoing_stream_multiplexer::operator()(stream_msg::ack_open&) {
  forward_to_upstream();
}

void outgoing_stream_multiplexer::operator()(stream_msg::batch&) {
  forward_to_downstream();
}

void outgoing_stream_multiplexer::operator()(stream_msg::ack_batch&) {
  forward_to_upstream();
}

void outgoing_stream_multiplexer::operator()(stream_msg::close&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  auto i = streams_.find(current_stream_msg_->sid);
  if (i != streams_.end()) {
    forward_to_downstream();
    streams_.erase(i);
  }
}

void outgoing_stream_multiplexer::operator()(stream_msg::abort& x) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  auto i = streams_.find(current_stream_msg_->sid);
  if (i != streams_.end()) {
    if (i->second.prev_stage == self_->current_sender())
      fail(x.reason, nullptr, i->second.next_stage);
    else
      fail(x.reason, i->second.prev_stage);
    streams_.erase(i);
  }
}

void outgoing_stream_multiplexer::operator()(stream_msg::downstream_failed&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
}

void outgoing_stream_multiplexer::operator()(stream_msg::upstream_failed&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
}

void outgoing_stream_multiplexer::forward_to_upstream() {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  manage_credit();
  send_local(current_stream_state_->second.prev_stage,
             std::move(*current_stream_msg_));
}

void outgoing_stream_multiplexer::forward_to_downstream() {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  send_remote(*current_stream_state_->second.rpath,
              std::move(*current_stream_msg_));
}

} // namespace detail
} // namespace caf

