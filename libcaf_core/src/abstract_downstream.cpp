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

#include <utility>

#include "caf/abstract_downstream.hpp"

#include "caf/send.hpp"
#include "caf/downstream_path.hpp"
#include "caf/downstream_policy.hpp"

namespace caf {

abstract_downstream::abstract_downstream(local_actor* selfptr,
                                         const stream_id& sid,
                                         std::unique_ptr<downstream_policy> ptr)
    : self_(selfptr),
      sid_(sid),
      min_buffer_size_(5), // TODO: make configurable
      policy_(std::move(ptr)) {
  // nop
}

abstract_downstream::~abstract_downstream() {
  // nop
}

long abstract_downstream::total_credit() const {
  return total_credit(paths_);
}

long abstract_downstream::max_credit() const {
  return max_credit(paths_);
}

long abstract_downstream::min_credit() const {
  return min_credit(paths_);
}

bool abstract_downstream::add_path(strong_actor_ptr ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  auto predicate = [&](const path_uptr& x) { return x->hdl == ptr; };
  if (std::none_of(paths_.begin(), paths_.end(), predicate)) {
    CAF_LOG_DEBUG("added new downstream path" << CAF_ARG(ptr));
    paths_.emplace_back(new path(std::move(ptr), false));
    return true;
  }
  return false;
}

bool abstract_downstream::confirm_path(const strong_actor_ptr& rebind_from,
                                       strong_actor_ptr& ptr,
                                       bool redeployable) {
  CAF_LOG_TRACE(CAF_ARG(rebind_from) << CAF_ARG(ptr) << CAF_ARG(redeployable));
  auto predicate = [&](const path_uptr& x) { return x->hdl == rebind_from; };
  auto e = paths_.end();
  auto i = std::find_if(paths_.begin(), e, predicate);
  if (i != e) {
    (*i)->redeployable = redeployable;
    if (rebind_from != ptr)
      (*i)->hdl = ptr;
    return true;
  }
  CAF_LOG_INFO("confirming path failed" << CAF_ARG(rebind_from)
               << CAF_ARG(ptr));
  return false;
}

bool abstract_downstream::remove_path(strong_actor_ptr& ptr) {
  auto predicate = [&](const path_uptr& x) { return x->hdl == ptr; };
  auto e = paths_.end();
  auto i = std::find_if(paths_.begin(), e, predicate);
  if (i != e) {
    CAF_ASSERT((*i)->hdl != nullptr);
    if (i != paths_.end() - 1)
      std::swap(*i, paths_.back());
    auto x = std::move(paths_.back());
    paths_.pop_back();
    unsafe_send_as(self_, x->hdl, make<stream_msg::close>(sid_));
    //policy_->reclaim(this, x);
    return true;
  }
  return false;
}

void abstract_downstream::close() {
  for (auto& x : paths_)
    unsafe_send_as(self_, x->hdl, make<stream_msg::close>(sid_));
  paths_.clear();
}

void abstract_downstream::abort(strong_actor_ptr& cause, const error& reason) {
  for (auto& x : paths_)
    if (x->hdl != cause)
      unsafe_send_as(self_, x->hdl,
                     make<stream_msg::abort>(this->sid_, reason));
}

abstract_downstream::path*
abstract_downstream::find(const strong_actor_ptr& ptr) const {
  return find(paths_, ptr);
}

long abstract_downstream::total_net_credit() const {
  return policy().total_net_credit(*this);
}

long abstract_downstream::num_paths() const {
  return static_cast<long>(paths_.size());
}

void abstract_downstream::send_batch(downstream_path& dest, long chunk_size,
                                     message chunk) {
  auto scs = static_cast<int32_t>(chunk_size);
  auto batch_id = dest.next_batch_id++;
  stream_msg::batch batch{scs, std::move(chunk), batch_id};
  if (dest.redeployable)
    dest.unacknowledged_batches.emplace_back(batch_id, batch);
  unsafe_send_as(self_, dest.hdl, stream_msg{sid_, std::move(batch)});
}

void abstract_downstream::sort_by_credit() {
  sort_by_credit(paths_);
}

} // namespace caf
