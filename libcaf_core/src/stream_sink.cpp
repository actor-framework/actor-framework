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

#include "caf/stream_sink.hpp"

#include "caf/send.hpp"
#include "caf/abstract_upstream.hpp"

namespace caf {

stream_sink::stream_sink(abstract_upstream* in_ptr,
                         strong_actor_ptr&& orig_sender,
                         std::vector<strong_actor_ptr>&& trailing_stages,
                         message_id mid)
    : in_ptr_(in_ptr),
      original_sender_(std::move(orig_sender)),
      next_stages_(std::move(trailing_stages)),
      original_msg_id_(mid) {
  // nop
}

bool stream_sink::done() const {
  // we are done streaming if no upstream paths remain
  return in_ptr_->closed();
}


error stream_sink::upstream_batch(strong_actor_ptr& hdl, size_t xs_size,
                                  message& xs) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(xs_size) << CAF_ARG(xs));
  auto path = in().find(hdl);
  if (path) {
    if (xs_size > path->assigned_credit)
      return sec::invalid_stream_state;
    path->assigned_credit -= xs_size;
    auto err = consume(xs);
    if (err == none)
      in().assign_credit(0, 0);
    return err;
  }
  return sec::invalid_upstream;
}

void stream_sink::abort(strong_actor_ptr& cause, const error& reason) {
  unsafe_send_as(in_ptr_->self(), original_sender_, reason);
  in_ptr_->abort(cause, reason);
}

void stream_sink::last_upstream_closed() {
  unsafe_response(in().self(), std::move(original_sender_),
                  std::move(next_stages_), original_msg_id_, finalize());
}

} // namespace caf
