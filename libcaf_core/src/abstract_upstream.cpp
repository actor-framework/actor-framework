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

#include "caf/abstract_upstream.hpp"

#include "caf/send.hpp"
#include "caf/stream_msg.hpp"

namespace caf {

abstract_upstream::abstract_upstream(local_actor* selfptr,
                                     abstract_upstream::policy_ptr p)
    : self_(selfptr),
      policy_(std::move(p)),
      continuous_(false) {
  // nop
}

abstract_upstream::~abstract_upstream() {
  // nop
}

void abstract_upstream::abort(strong_actor_ptr& cause, const error& reason) {
  for (auto& x : paths_)
    if (x->hdl != cause)
      unsafe_send_as(self_, x->hdl, make<stream_msg::abort>(x->sid, reason));
}

void abstract_upstream::assign_credit(size_t buf_size,
                                      size_t downstream_credit) {
  policy_->assign_credit(policy_vec_, buf_size, downstream_credit);
  for (auto& x : policy_vec_) {
    auto n = x.second;
    if (n > 0) {
      auto ptr = x.first;
      ptr->assigned_credit += n;
      unsafe_send_as(self_, ptr->hdl, make<stream_msg::ack_batch>(
                                        ptr->sid, static_cast<int32_t>(n),
                                        ptr->last_batch_id++));
    }
  }
}

expected<size_t> abstract_upstream::add_path(strong_actor_ptr hdl,
                                             const stream_id& sid,
                                             stream_priority prio,
                                             size_t buf_size,
                                             size_t downstream_credit) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(sid) << CAF_ARG(prio));
  CAF_ASSERT(hdl != nullptr);
  if (!find(hdl)) {
    CAF_LOG_DEBUG("add new upstraem path" << CAF_ARG(hdl));
    paths_.emplace_back(new path(std::move(hdl), sid, prio));
    policy_vec_.emplace_back(paths_.back().get(), 0);
    // use a one-shot actor to calculate initial credit
    upstream_policy::assignment_vec tmp;
    tmp.emplace_back(paths_.back().get(), 0);
    policy_->assign_credit(tmp, buf_size, downstream_credit);
    paths_.back()->assigned_credit += tmp.back().second;
    return tmp.back().second;
  }
  return sec::upstream_already_exists;
}

bool abstract_upstream::remove_path(const strong_actor_ptr& hdl) {
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
    auto e2 = policy_vec_.end();
    auto i2 = std::find_if(policy_vec_.begin(), e2, has_ptr);
    if (i2 != e2) {
      std::swap(*i2, policy_vec_.back());
      policy_vec_.pop_back();
    }
    // Drop path from list.
    if (i != e - 1)
      std::swap(*i, paths_.back());
    paths_.pop_back();
    return true;
  }
  return false;
}

auto abstract_upstream::find(const strong_actor_ptr& x) const
-> optional<path&> {
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
