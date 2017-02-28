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

#include "caf/detail/incoming_stream_multiplexer.hpp"

#include "caf/send.hpp"
#include "caf/variant.hpp"
#include "caf/to_string.hpp"
#include "caf/local_actor.hpp"

namespace caf {
namespace detail {

incoming_stream_multiplexer::incoming_stream_multiplexer(local_actor* self,
                                                         backend& service)
    : stream_multiplexer(self, service) {
  // nop
}

void incoming_stream_multiplexer::operator()(stream_msg& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(self_->current_mailbox_element() != nullptr);
  dispatch(*this, x);
}

void incoming_stream_multiplexer::operator()(stream_msg::open& x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(current_stream_msg_ != nullptr);
  auto prev = std::move(x.prev_stage);
  // Make sure we have a previous stage.
  if (!prev) {
    CAF_LOG_WARNING("received stream_msg::open without previous stage");
    return fail(sec::invalid_upstream, nullptr);
  }
  // Make sure we have a next stage.
  auto cme = self_->current_mailbox_element();
  if (!cme || cme->stages.empty()) {
    CAF_LOG_WARNING("received stream_msg::open without next stage");
    return fail(sec::invalid_downstream, std::move(prev));
  }
  auto successor = cme->stages.back();
  cme->stages.pop_back();
  // Our prev always is the remote stream server proxy.
  auto current_remote_path = remotes().emplace(prev->node(), prev).first;
  current_stream_state_ = streams_.emplace(current_stream_msg_->sid,
                                           stream_state{std::move(prev),
                                           successor,
                                           &current_remote_path->second}).first;
  // Rewrite handshake and forward it to the next stage.
  x.prev_stage = self_->ctrl();
  auto ptr = make_mailbox_element(
    cme->sender, cme->mid, std::move(cme->stages),
    make<stream_msg::open>(current_stream_msg_->sid, std::move(x.msg),
                           self_->ctrl(), successor, x.priority,
                           x.redeployable));
  successor->enqueue(std::move(ptr), self_->context());
  // Send out demand upstream.
  manage_credit();
}

void incoming_stream_multiplexer::operator()(stream_msg::ack_open&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  forward_to_upstream();
}

void incoming_stream_multiplexer::operator()(stream_msg::batch&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  forward_to_downstream();
}

void incoming_stream_multiplexer::operator()(stream_msg::ack_batch&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  forward_to_upstream();
}

void incoming_stream_multiplexer::operator()(stream_msg::close&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  forward_to_downstream();
  streams_.erase(current_stream_state_);
}

void incoming_stream_multiplexer::operator()(stream_msg::abort& x) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  if (current_stream_state_->second.prev_stage == self_->current_sender())
    fail(x.reason, nullptr, current_stream_state_->second.next_stage);
  else
    fail(x.reason, current_stream_state_->second.prev_stage);
  streams_.erase(current_stream_state_);
}

void incoming_stream_multiplexer::operator()(stream_msg::downstream_failed&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  // TODO: implement me
}

void incoming_stream_multiplexer::operator()(stream_msg::upstream_failed&) {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  // TODO: implement me
}

void incoming_stream_multiplexer::forward_to_upstream() {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  CAF_ASSERT(current_stream_state_ != streams_.end());
  send_remote(*current_stream_state_->second.rpath,
              std::move(*current_stream_msg_));
}

void incoming_stream_multiplexer::forward_to_downstream() {
  CAF_ASSERT(current_stream_msg_ != nullptr);
  // When forwarding downstream, we also have to manage upstream credit.
  manage_credit();
  send_local(current_stream_state_->second.next_stage,
             std::move(*current_stream_msg_));
}

} // namespace detail
} // namespace caf

