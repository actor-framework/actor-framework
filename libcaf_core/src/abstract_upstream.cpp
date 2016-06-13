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

#include "caf/abstract_upstream.hpp"

#include "caf/send.hpp"
#include "caf/stream_msg.hpp"

namespace caf {

abstract_upstream::abstract_upstream(local_actor* selfptr,
                                     abstract_upstream::policy_ptr p)
    : self_(selfptr),
      policy_(std::move(p)) {
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

error abstract_upstream::pull(strong_actor_ptr& hdl, size_t n) {
  CAF_ASSERT(hdl != nullptr);
  auto has_hdl = [&](const path_uptr& x) {
    CAF_ASSERT(x != nullptr);
    return x->hdl == hdl;
  };
  auto e = paths_.end();
  auto i = std::find_if(paths_.begin(), e, has_hdl);
  if (i != e) {
    auto& x = *(*i);
    CAF_ASSERT(x.hdl == hdl);
    x.assigned_credit += n;
    unsafe_send_as(self_, x.hdl,
                   make<stream_msg::ack_batch>(x.sid, static_cast<int32_t>(n),
                                               x.last_batch_id++));
    return none;
  }
  return sec::invalid_upstream;
}

error abstract_upstream::pull(size_t n) {
  // TODO: upstream policy
  if (!paths_.empty()) {
    pull(paths_.front()->hdl, n);
    return none;
  }
  return sec::invalid_upstream;
}

expected<size_t> abstract_upstream::add_path(strong_actor_ptr hdl,
                                             const stream_id& sid,
                                             stream_priority prio) {
  CAF_ASSERT(hdl != nullptr);
  auto has_hdl = [&](const path_uptr& x) {
    CAF_ASSERT(x != nullptr);
    return x->hdl == hdl;
  };
  auto e = paths_.end();
  auto i = std::find_if(paths_.begin(), e, has_hdl);
  if (i == e) {
    paths_.emplace_back(new path(std::move(hdl), sid, prio));
    return policy_->initial_credit(*this, *paths_.back());
  }
  return sec::upstream_already_exists;
}

bool abstract_upstream::remove_path(const strong_actor_ptr& hdl) {
  auto has_hdl = [&](const path_uptr& x) {
    CAF_ASSERT(x != nullptr);
    return x->hdl == hdl;
  };
  auto e = paths_.end();
  auto i = std::find_if(paths_.begin(), e, has_hdl);
  if (i != e) {
    if (i != e - 1)
      std::swap(*i, paths_.back());
    paths_.pop_back();
    return true;
  }
  return false;
}

} // namespace caf
