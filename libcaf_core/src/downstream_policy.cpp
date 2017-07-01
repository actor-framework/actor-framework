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

#include "caf/downstream_policy.hpp"

#include <utility>

#include "caf/send.hpp"
#include "caf/atom.hpp"
#include "caf/stream_aborter.hpp"
#include "caf/downstream_path.hpp"
#include "caf/downstream_policy.hpp"

namespace caf {

downstream_policy::downstream_policy(local_actor* selfptr, const stream_id& id)
    : self_(selfptr),
      sid_(id),
      // TODO: make configurable
      min_batch_size_(1),
      max_batch_size_(5),
      min_buffer_size_(5),
      max_batch_delay_(infinite) {
  // nop
}

downstream_policy::~downstream_policy() {
  // nop
}

long downstream_policy::total_credit() const {
  return total_credit(paths_);
}

long downstream_policy::max_credit() const {
  return max_credit(paths_);
}

long downstream_policy::min_credit() const {
  return min_credit(paths_);
}

bool downstream_policy::add_path(strong_actor_ptr ptr) {
  CAF_LOG_TRACE(CAF_ARG(ptr));
  auto predicate = [&](const path_uptr& x) { return x->hdl == ptr; };
  if (std::none_of(paths_.begin(), paths_.end(), predicate)) {
    CAF_LOG_DEBUG("added new downstream path" << CAF_ARG(ptr));
    stream_aborter::add(ptr, self_->address(), sid_);
    paths_.emplace_back(new downstream_path(std::move(ptr), false));
    return true;
  }
  return false;
}

bool downstream_policy::confirm_path(const strong_actor_ptr& rebind_from,
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

bool downstream_policy::remove_path(strong_actor_ptr& ptr) {
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
    stream_aborter::del(x->hdl, self_->address(), sid_);
    return true;
  }
  return false;
}

void downstream_policy::close() {
  for (auto& x : paths_)
    unsafe_send_as(self_, x->hdl, make<stream_msg::close>(sid_));
  paths_.clear();
}

void downstream_policy::abort(strong_actor_ptr& cause, const error& reason) {
  CAF_LOG_TRACE(CAF_ARG(cause) << CAF_ARG(reason));
  for (auto& x : paths_) {
    if (x->hdl != cause)
      unsafe_send_as(self_, x->hdl, make<stream_msg::abort>(sid_, reason));
    stream_aborter::del(x->hdl, self_->address(), sid_);
  }
}

downstream_path* downstream_policy::find(const strong_actor_ptr& ptr) const {
  return find(paths_, ptr);
}

void downstream_policy::sort_paths_by_credit() {
  sort_by_credit(paths_);
}

void downstream_policy::emit_batch(downstream_path& dest, size_t xs_size,
                                   message xs) {
  CAF_LOG_TRACE(CAF_ARG(dest) << CAF_ARG(xs_size) << CAF_ARG(xs));
  auto scs = static_cast<int32_t>(xs_size);
  auto batch_id = dest.next_batch_id++;
  stream_msg::batch batch{scs, std::move(xs), batch_id};
  if (dest.redeployable)
    dest.unacknowledged_batches.emplace_back(batch_id, batch);
  unsafe_send_as(self_, dest.hdl, stream_msg{sid_, std::move(batch)});
}

} // namespace caf
