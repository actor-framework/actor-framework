/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/locks.hpp"

#include "caf/atom.hpp"
#include "caf/to_string.hpp"
#include "caf/message.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/exit_reason.hpp"

#include "caf/detail/singletons.hpp"

namespace caf {

actor_proxy::anchor::anchor(actor_proxy* instance) : ptr_(instance) {
  // nop
}

actor_proxy::anchor::~anchor() {
  // nop
}

bool actor_proxy::anchor::expired() const {
  return ! ptr_.load();
}

actor_proxy_ptr actor_proxy::anchor::get() {
  actor_proxy_ptr result;
  { // lifetime scope of guard
    shared_lock<detail::shared_spinlock> guard{lock_};
    auto ptr = ptr_.load();
    if (ptr) {
      result.reset(ptr);
    }
  }
  return result;
}

bool actor_proxy::anchor::try_expire() noexcept {
  std::lock_guard<detail::shared_spinlock> guard{lock_};
  // double-check reference count
  if (ptr_.load()->get_reference_count() == 0) {
    ptr_ = nullptr;
    return true;
  }
  return false;
}

actor_proxy::~actor_proxy() {
  // nop
}

actor_proxy::actor_proxy(actor_id aid, node_id nid)
    : abstract_actor(aid, nid),
      anchor_(make_counted<anchor>(this)) {
  // nop
}

void actor_proxy::request_deletion(bool decremented_rc) noexcept {
  // make sure ref count is 0 because try_expire might leak otherwise
  if (! decremented_rc && rc_.fetch_sub(1, std::memory_order_acq_rel) != 1) {
    // deletion request canceled due to a weak ptr access
    return;
  }
  if (anchor_->try_expire()) {
    delete this;
  }
}

} // namespace caf
