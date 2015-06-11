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

#include "caf/detail/actor_registry.hpp"

#include <mutex>
#include <limits>
#include <stdexcept>

#include "caf/locks.hpp"
#include "caf/attachable.hpp"
#include "caf/exit_reason.hpp"

#include "caf/scheduler/detached_threads.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/shared_spinlock.hpp"

namespace caf {
namespace detail {

namespace {

using exclusive_guard = unique_lock<shared_spinlock>;
using shared_guard = shared_lock<shared_spinlock>;

} // namespace <anonymous>

actor_registry::~actor_registry() {
  // nop
}

actor_registry::actor_registry() : running_(0), ids_(1) {
  // nop
}

actor_registry::value_type actor_registry::get_entry(actor_id key) const {
  shared_guard guard(instances_mtx_);
  auto i = entries_.find(key);
  if (i != entries_.end()) {
    return i->second;
  }
  CAF_LOG_DEBUG("key not found, assume the actor no longer exists: " << key);
  return {nullptr, exit_reason::unknown};
}

void actor_registry::put(actor_id key, const abstract_actor_ptr& val) {
  if (val == nullptr) {
    return;
  }
  { // lifetime scope of guard
    exclusive_guard guard(instances_mtx_);
    if (! entries_.emplace(key,
                           value_type{val, exit_reason::not_exited}).second) {
      // already defined
      return;
    }
  }
  // attach functor without lock
  CAF_LOG_INFO("added actor with ID " << key);
  actor_registry* reg = this;
  val->attach_functor([key, reg](uint32_t reason) { reg->erase(key, reason); });
}

void actor_registry::erase(actor_id key, uint32_t reason) {
  exclusive_guard guard(instances_mtx_);
  auto i = entries_.find(key);
  if (i != entries_.end()) {
    auto& entry = i->second;
    CAF_LOG_INFO("erased actor with ID " << key << ", reason " << reason);
    entry.first = nullptr;
    entry.second = reason;
  }
}

uint32_t actor_registry::next_id() {
  return ++ids_;
}

void actor_registry::inc_running() {
# if defined(CAF_LOG_LEVEL) && CAF_LOG_LEVEL >= CAF_DEBUG
    CAF_LOG_DEBUG("new value = " << ++running_);
# else
    ++running_;
# endif
}

size_t actor_registry::running() const {
  return running_.load();
}

void actor_registry::dec_running() {
  size_t new_val = --running_;
  if (new_val <= 1) {
    std::unique_lock<std::mutex> guard(running_mtx_);
    running_cv_.notify_all();
  }
  CAF_LOG_DEBUG(CAF_ARG(new_val));
}

void actor_registry::await_running_count_equal(size_t expected) {
  CAF_ASSERT(expected == 0 || expected == 1);
  CAF_LOG_TRACE(CAF_ARG(expected));
  std::unique_lock<std::mutex> guard{running_mtx_};
  while (running_ != expected) {
    CAF_LOG_DEBUG("count = " << running_.load());
    running_cv_.wait(guard);
  }
}

} // namespace detail
} // namespace caf
