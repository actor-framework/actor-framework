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

#include "caf/stream_gatherer_impl.hpp"

namespace caf {

stream_gatherer_impl::stream_gatherer_impl(local_actor* selfptr)
    : super(selfptr),
      high_watermark_(40),
      min_credit_assignment_(1),
      max_credit_(50) {
  // nop
}

stream_gatherer_impl::~stream_gatherer_impl() {
  // nop
}

stream_gatherer::path_ptr
stream_gatherer_impl::add_path(const stream_id& sid, strong_actor_ptr hdl,
                               strong_actor_ptr original_stage,
                               stream_priority prio, long available_credit,
                               bool redeployable, response_promise result_cb) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(hdl) << CAF_ARG(original_stage)
                << CAF_ARG(prio) << CAF_ARG(available_credit));
  CAF_ASSERT(hdl != nullptr);
  if (find(sid, hdl) != nullptr) {
    inbound_path::emit_irregular_shutdown(self_, sid, hdl,
                                          sec::cannot_add_upstream);

    return nullptr;
  }
  auto ptr = add_path_impl(sid, std::move(hdl));
  CAF_ASSERT(ptr != nullptr);
  assignment_vec_.emplace_back(ptr, 0l);
  if (result_cb.pending())
    listeners_.emplace_back(std::move(result_cb));
  ptr->prio = prio;
  ptr->emit_ack_open(actor_cast<actor_addr>(original_stage),
                     initial_credit(available_credit, ptr), redeployable);
  return ptr;
}

bool stream_gatherer_impl::remove_path(const stream_id& sid,
                                       const actor_addr& x, error reason,
                                       bool silent) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(x)
                << CAF_ARG(reason) << CAF_ARG(silent));
  auto pred = [&](const assignment_pair& y) {
    return y.first->sid == sid && y.first->hdl == x;
  };
  auto e = assignment_vec_.end();
  auto i = std::find_if(assignment_vec_.begin(), e, pred);
  if (i != e) {
    assignment_vec_.erase(i);
    return super::remove_path(sid, x, std::move(reason), silent);
  }
  return false;
}

void stream_gatherer_impl::close(message result) {
  CAF_LOG_TRACE(CAF_ARG(result) << CAF_ARG2("remaining paths", paths_.size())
                << CAF_ARG2("listener", listeners_.size()));
  for (auto& path : paths_)
    stream_aborter::del(path->hdl, self_->address(), path->sid, aborter_type);
  paths_.clear();
  for (auto& listener : listeners_)
    listener.deliver(result);
  listeners_.clear();
}

void stream_gatherer_impl::abort(error reason) {
  for (auto& path : paths_) {
    stream_aborter::del(path->hdl, self_->address(), path->sid, aborter_type);
    path->shutdown_reason = reason;
  }
  paths_.clear();
  for (auto& listener : listeners_)
    listener.deliver(reason);
  listeners_.clear();
}

long stream_gatherer_impl::high_watermark() const {
  return high_watermark_;
}

long stream_gatherer_impl::min_credit_assignment() const {
  return min_credit_assignment_;
}

long stream_gatherer_impl::max_credit() const {
  return max_credit_;
}

void stream_gatherer_impl::high_watermark(long x) {
  high_watermark_ = x;
}

void stream_gatherer_impl::min_credit_assignment(long x) {
  min_credit_assignment_ = x;
}

void stream_gatherer_impl::max_credit(long x) {
  max_credit_ = x;
}

void stream_gatherer_impl::emit_credits() {
  for (auto& kvp : assignment_vec_)
    if (kvp.second > 0)
      kvp.first->emit_ack_batch(kvp.second);
}

} // namespace caf
