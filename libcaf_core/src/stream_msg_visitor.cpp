/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
#include "caf/inbound_path.hpp"
#include "caf/outbound_path.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf {

stream_msg_visitor::stream_msg_visitor(scheduled_actor* self,
                                       const stream_msg& msg, behavior* bhvr)
    : self_(self),
      sid_(msg.sid),
      sender_(msg.sender),
      bhvr_(bhvr) {
  CAF_ASSERT(sender_ != nullptr);
}

auto stream_msg_visitor::operator()(stream_msg::open& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  CAF_ASSERT(x.prev_stage != nullptr);
  CAF_ASSERT(x.original_stage != nullptr);
  auto& predecessor = x.prev_stage;
  // Convenience function for aborting the stream on error.
  auto fail = [&](error err) -> result_type {
    inbound_path::emit_irregular_shutdown(self_, sid_, predecessor,
                                          std::move(err));
    return false;
  };
  // Sanity checks.
  if (!predecessor) {
    CAF_LOG_WARNING("received stream_msg::open with empty prev_stage");
    return fail(sec::invalid_upstream);
  }
  if (bhvr_ == nullptr) {
    CAF_LOG_WARNING("received stream_msg::open with empty behavior");
    return fail(sec::stream_init_failed);
  }
  if (self_->streams().count(sid_) != 0) {
    CAF_LOG_WARNING("received duplicate stream_msg::open");
    return fail(sec::stream_init_failed);
  }
  // Invoke behavior of parent to perform handshake.
  (*bhvr_)(x.msg);
  if (self_->streams().count(sid_) == 0) {
    CAF_LOG_WARNING("actor did not provide a stream "
                    "handler after receiving handshake:"
                    << CAF_ARG(x.msg));
    return fail(sec::stream_init_failed);
  }
  return true;
}

auto stream_msg_visitor::operator()(stream_msg::close&) -> result_type {
  CAF_LOG_TRACE("");
  return invoke([&](stream_manager_ptr& mgr) {
    return mgr->close(sid_, sender_);
  });
}

auto stream_msg_visitor::operator()(stream_msg::drop&) -> result_type {
  CAF_LOG_TRACE("");
  return invoke([&](stream_manager_ptr& mgr) {
    return mgr->drop(sid_, sender_);
  });
}

auto stream_msg_visitor::operator()(stream_msg::forced_close& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  return invoke([&](stream_manager_ptr& mgr) {
    return mgr->forced_close(sid_, sender_, std::move(x.reason));
  });
}

auto stream_msg_visitor::operator()(stream_msg::forced_drop& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  return invoke([&](stream_manager_ptr& mgr) {
    return mgr->forced_drop(sid_, sender_, std::move(x.reason));
  });
}

auto stream_msg_visitor::operator()(stream_msg::ack_open& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  return invoke([&](stream_manager_ptr& mgr) {
    return mgr->ack_open(sid_, x.rebind_from, std::move(x.rebind_to),
                         x.initial_demand, x.redeployable);
  });
}

auto stream_msg_visitor::operator()(stream_msg::batch& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  return invoke([&](stream_manager_ptr& mgr) {
    return mgr->batch(sid_, sender_, static_cast<long>(x.xs_size), x.xs, x.id);
  });
}

auto stream_msg_visitor::operator()(stream_msg::ack_batch& x) -> result_type {
  CAF_LOG_TRACE(CAF_ARG(x));
  return invoke([&](stream_manager_ptr& mgr) {
    return mgr->ack_batch(sid_, sender_, static_cast<long>(x.new_capacity),
                          x.acknowledged_id);
  });
}

} // namespace caf
