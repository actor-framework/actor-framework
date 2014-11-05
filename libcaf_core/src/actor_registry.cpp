/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include <mutex>
#include <limits>
#include <stdexcept>

#include "caf/attachable.hpp"
#include "caf/exit_reason.hpp"
#include "caf/detail/actor_registry.hpp"

#include "caf/locks.hpp"
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

actor_registry::actor_registry() : m_running(0), m_ids(1) {
  // nop
}

actor_registry::value_type actor_registry::get_entry(actor_id key) const {
  shared_guard guard(m_instances_mtx);
  auto i = m_entries.find(key);
  if (i != m_entries.end()) {
    return i->second;
  }
  CAF_LOG_DEBUG("key not found, assume the actor no longer exists: " << key);
  return {nullptr, exit_reason::unknown};
}

void actor_registry::put(actor_id key, const abstract_actor_ptr& val) {
  if (val == nullptr) {
    return;
  }
  auto entry = std::make_pair(key, value_type(val, exit_reason::not_exited));
  { // lifetime scope of guard
    exclusive_guard guard(m_instances_mtx);
    if (!m_entries.insert(entry).second) {
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
  exclusive_guard guard(m_instances_mtx);
  auto i = m_entries.find(key);
  if (i != m_entries.end()) {
    auto& entry = i->second;
    CAF_LOG_INFO("erased actor with ID " << key << ", reason " << reason);
    entry.first = nullptr;
    entry.second = reason;
  }
}

uint32_t actor_registry::next_id() {
  return ++m_ids;
}

void actor_registry::inc_running() {
# if CAF_LOG_LEVEL >= CAF_DEBUG
    CAF_LOG_DEBUG("new value = " << ++m_running);
# else
    ++m_running;
# endif
}

size_t actor_registry::running() const {
  return m_running.load();
}

void actor_registry::dec_running() {
  size_t new_val = --m_running;
  if (new_val <= 1) {
    std::unique_lock<std::mutex> guard(m_running_mtx);
    m_running_cv.notify_all();
  }
  CAF_LOG_DEBUG(CAF_ARG(new_val));
}

void actor_registry::await_running_count_equal(size_t expected) {
  CAF_REQUIRE(expected == 0 || expected == 1);
  CAF_LOG_TRACE(CAF_ARG(expected));
  std::unique_lock<std::mutex> guard{m_running_mtx};
  while (m_running != expected) {
    CAF_LOG_DEBUG("count = " << m_running.load());
    m_running_cv.wait(guard);
  }
}

} // namespace detail
} // namespace caf
