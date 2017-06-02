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

#include "caf/upstream_policy.hpp"

#include "caf/send.hpp"
#include "caf/stream_msg.hpp"
#include "caf/upstream_path.hpp"

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

upstream_policy::upstream_policy(local_actor* selfptr)
    : self_(selfptr),
      continuous_(false),
      high_watermark_(5),
      min_credit_assignment_(1),
      max_credit_(5) {
  // nop
}

upstream_policy::~upstream_policy() {
  // nop
}

// -- path management ----------------------------------------------------------


void upstream_policy::abort(strong_actor_ptr& cause, const error& reason) {
  for (auto& x : paths_)
    if (x->hdl != cause)
      unsafe_send_as(self_, x->hdl, make<stream_msg::abort>(x->sid, reason));
}

void upstream_policy::assign_credit(long downstream_capacity) {
  CAF_LOG_TRACE(CAF_ARG(downstream_capacity));
  auto used_capacity = 0l;
  for (auto& x : assignment_vec_)
    used_capacity += static_cast<long>(x.first->assigned_credit);
  CAF_LOG_DEBUG(CAF_ARG(used_capacity));
  if (used_capacity >= downstream_capacity)
    return;
  fill_assignment_vec(downstream_capacity - used_capacity);
  for (auto& x : assignment_vec_) {
    auto n = x.second;
    if (n > 0) {
      auto ptr = x.first;
      ptr->assigned_credit += n;
      CAF_LOG_DEBUG("ack batch" << ptr->last_batch_id
                    << "with" << n << "new capacity");
      unsafe_send_as(self_, ptr->hdl,
                     make<stream_msg::ack_batch>(ptr->sid,
                                                 static_cast<int32_t>(n),
                                                 ptr->last_batch_id++));
    }
  }
}

expected<long> upstream_policy::add_path(strong_actor_ptr hdl,
                                         const stream_id& sid,
                                         stream_priority prio,
                                         long downstream_credit) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(sid) << CAF_ARG(prio)
                << CAF_ARG(downstream_credit));
  CAF_ASSERT(hdl != nullptr);
  if (!find(hdl)) {
    auto ptr = new upstream_path(std::move(hdl), sid, prio);
    paths_.emplace_back(ptr);
    assignment_vec_.emplace_back(ptr, 0);
    if (downstream_credit > 0)
      ptr->assigned_credit = std::min(max_credit_, downstream_credit);
    CAF_LOG_DEBUG("add new upstraem path" << ptr->hdl 
                  << "with initial credit" << ptr->assigned_credit);
    return ptr->assigned_credit;
  }
  return sec::upstream_already_exists;
}

bool upstream_policy::remove_path(const strong_actor_ptr& hdl) {
  CAF_LOG_TRACE(CAF_ARG(hdl));
  // Find element in our paths list.
  auto has_hdl = [&](const path_uptr& x) {
    CAF_ASSERT(x != nullptr);
    return x->hdl == hdl;
  };
  auto e = paths_.end();
  auto i = std::find_if(paths_.begin(), e, has_hdl);
  if (i != e) {
    // Also find and erase this element from our policy vector.
    auto has_ptr = [&](const upstream_policy::assignment_pair& p) {
      return p.first == i->get();
    };
    auto e2 = assignment_vec_.end();
    auto i2 = std::find_if(assignment_vec_.begin(), e2, has_ptr);
    if (i2 != e2) {
      std::swap(*i2, assignment_vec_.back());
      assignment_vec_.pop_back();
    }
    // Drop path from list.
    if (i != e - 1)
      std::swap(*i, paths_.back());
    paths_.pop_back();
    return true;
  }
  return false;
}

upstream_path* upstream_policy::find(const strong_actor_ptr& x) const {
  CAF_ASSERT(x != nullptr);
  auto pred = [&](const path_uptr& y) {
    CAF_ASSERT(y != nullptr);
    return x == y->hdl;
  };
  auto e = paths_.end();
  auto i = std::find_if(paths_.begin(), e, pred);
  return i != e ? i->get() : nullptr;
}

} // namespace caf
