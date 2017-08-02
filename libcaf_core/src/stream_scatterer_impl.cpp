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

#include "caf/stream_scatterer_impl.hpp"

#include "caf/logger.hpp"
#include "caf/outbound_path.hpp"

namespace caf {

stream_scatterer_impl::stream_scatterer_impl(local_actor* selfptr)
    : super(selfptr),
      min_batch_size_(1),
      max_batch_size_(150),
      min_buffer_size_(50),
      max_batch_delay_(infinite) {
  // nop
}

stream_scatterer_impl::~stream_scatterer_impl() {
  // nop
}

stream_scatterer::path_ptr
stream_scatterer_impl::add_path(const stream_id& sid, strong_actor_ptr origin,
                                strong_actor_ptr sink_ptr,
                                mailbox_element::forwarding_stack stages,
                                message_id handshake_mid,
                                message handshake_data, stream_priority prio,
                                bool redeployable) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(origin) << CAF_ARG(sink_ptr)
                << CAF_ARG(stages) << CAF_ARG(handshake_mid)
                << CAF_ARG(handshake_data) << CAF_ARG(prio)
                << CAF_ARG(redeployable));
  auto ptr = add_path_impl(sid, std::move(sink_ptr));
  if (ptr != nullptr)
    ptr->emit_open(std::move(origin), std::move(stages), handshake_mid,
                   std::move(handshake_data), prio, redeployable);
  return ptr;
}

stream_scatterer::path_ptr
stream_scatterer_impl::confirm_path(const stream_id& sid,
                                    const actor_addr& from, strong_actor_ptr to,
                                    long initial_demand, bool redeployable) {
  CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(from) << CAF_ARG(to)
                << CAF_ARG(initial_demand) << CAF_ARG(redeployable));
  auto ptr = find(sid, from);
  if (ptr == nullptr) {
    CAF_LOG_WARNING("cannot confirm unknown path");
    outbound_path::emit_irregular_shutdown(self_, sid, std::move(to),
                                           sec::invalid_downstream);
    return nullptr;
  }
  if (from != to)
    ptr->hdl = std::move(to);
  ptr->redeployable = redeployable;
  ptr->open_credit += initial_demand;
  return ptr;
}

bool stream_scatterer_impl::paths_clean() const {
  auto is_clean = [](const path_uptr& x) {
    return x->next_ack_id == x->next_batch_id;
  };
  return buffered() == 0 && std::all_of(paths_.begin(), paths_.end(), is_clean);
}

void stream_scatterer_impl::close() {
  CAF_LOG_TRACE("");
  for (auto& path : paths_)
    stream_aborter::del(path->hdl, self_->address(), path->sid, aborter_type);
  paths_.clear();
}

void stream_scatterer_impl::abort(error reason) {
  for (auto& path : paths_) {
    stream_aborter::del(path->hdl, self_->address(), path->sid, aborter_type);
    path->shutdown_reason = reason;
  }
  paths_.clear();
}

long stream_scatterer_impl::total_credit() const {
  return total_credit(paths_);
}

long stream_scatterer_impl::min_credit() const {
  return min_credit(paths_);
}

long stream_scatterer_impl::max_credit() const {
  return max_credit(paths_);
}

long stream_scatterer_impl::min_batch_size() const {
  return min_batch_size_;
}

long stream_scatterer_impl::max_batch_size() const {
  return max_batch_size_;
}

long stream_scatterer_impl::min_buffer_size() const {
  return min_buffer_size_;
}

duration stream_scatterer_impl::max_batch_delay() const {
  return max_batch_delay_;
}

void stream_scatterer_impl::min_batch_size(long x) {
  min_batch_size_ = x;
}

void stream_scatterer_impl::max_batch_size(long x) {
  max_batch_size_ = x;
}

void stream_scatterer_impl::min_buffer_size(long x) {
  min_buffer_size_ = x;
}

void stream_scatterer_impl::max_batch_delay(duration x) {
  max_batch_delay_ = std::move(x);
}

} // namespace caf
