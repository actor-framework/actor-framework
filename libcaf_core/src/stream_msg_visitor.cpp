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

#include "caf/stream_msg_visitor.hpp"

#include "caf/send.hpp"
#include "caf/logger.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf {

stream_msg_visitor::stream_msg_visitor(scheduled_actor* self, stream_id& sid,
                                       iterator i, iterator last)
    : self_(self),
      sid_(sid),
      i_(i),
      e_(last) {
  // nop
}

auto stream_msg_visitor::operator()(stream_msg::open& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(self_->current_mailbox_element() != nullptr);
  if (i_ != e_)
    return {sec::downstream_already_exists, e_};
  auto& predecessor = x.prev_stage;
  auto fail = [&](error reason) -> result_type {
    unsafe_send_as(self_, predecessor, make<stream_msg::abort>(sid_, reason));
    auto rp = self_->make_response_promise();
    rp.deliver(reason);
    return {std::move(reason), e_};
  };
  if (!predecessor) {
    CAF_LOG_WARNING("received stream_msg::open with empty prev_stage");
    return fail(sec::invalid_upstream);
  }
  auto& bs = self_->bhvr_stack();
  if (bs.empty()) {
    CAF_LOG_WARNING("cannot open stream in actor without behavior");
    return fail(sec::stream_init_failed);
  }
  auto bhvr = self_->bhvr_stack().back();
  auto res = bhvr(x.token);
  if (!res) {
    CAF_LOG_WARNING("stream handshake failed: actor did not respond to token:"
                    << CAF_ARG(x.token));
    return fail(sec::stream_init_failed);
  }
  i_ = self_->streams().find(sid_);
  if (i_ == e_) {
    CAF_LOG_WARNING("stream handshake failed: actor did not provide a stream "
                    "handler after receiving token:"
                    << CAF_ARG(x.token));
    return fail(sec::stream_init_failed);
  }
  auto& handler = i_->second;
  // store upstream actor
  auto initial_credit = handler->add_upstream(x.prev_stage, sid_, x.priority);
  if (initial_credit) {
    // check whether we are a stage in a longer pipeline and send more
    // stream_open messages if required
    auto ic = static_cast<int32_t>(*initial_credit);
    std::vector<atom_value> filter;
    unsafe_send_as(self_, predecessor,
                   make_message(make<stream_msg::ack_open>(
                     std::move(sid_), ic, std::move(filter), false)));
    return {none, i_};
  }
  printf("error: %s\n", self_->system().render(initial_credit.error()).c_str());
  self_->streams().erase(i_);
  return fail(std::move(initial_credit.error()));
}

auto stream_msg_visitor::operator()(stream_msg::close&) -> result_type {
  CAF_LOG_TRACE("");
  if (i_ != e_) {
    i_->second->close_upstream(self_->current_sender());
    return {none, i_->second->done() ? i_ : e_};
  }
  return {sec::invalid_upstream, e_};
}

auto stream_msg_visitor::operator()(stream_msg::abort& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  if (i_ == e_) {
    CAF_LOG_DEBUG("received stream_msg::abort for unknown stream");
    return {sec::unexpected_message, e_};
  }
  return {std::move(x.reason), i_};
}

auto stream_msg_visitor::operator()(stream_msg::ack_open& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  if (i_ != e_)
    return {i_->second->add_downstream(self_->current_sender(), x.filter,
                                       static_cast<size_t>(x.initial_demand),
                                       false),
            i_};
  CAF_LOG_DEBUG("received stream_msg::ok for unknown stream");
  return {sec::unexpected_message, e_};
}

auto stream_msg_visitor::operator()(stream_msg::batch& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  if (i_ != e_)
    return {i_->second->upstream_batch(self_->current_sender(),
                                       static_cast<size_t>(x.xs_size), x.xs),
            i_};
  CAF_LOG_DEBUG("received stream_msg::batch for unknown stream");
  return {sec::unexpected_message, e_};
}

auto stream_msg_visitor::operator()(stream_msg::ack_batch& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  if (i_ != e_)
    return {i_->second->downstream_demand(self_->current_sender(),
                                          static_cast<size_t>(x.new_capacity)),
            i_};
  CAF_LOG_DEBUG("received stream_msg::batch for unknown stream");
  return {sec::unexpected_message, e_};
}

auto stream_msg_visitor::operator()(stream_msg::downstream_failed&) -> result_type {
  // TODO
  return {none, e_};
}

auto stream_msg_visitor::operator()(stream_msg::upstream_failed&) -> result_type {
  // TODO
  return {none, e_};
}

} // namespace caf
