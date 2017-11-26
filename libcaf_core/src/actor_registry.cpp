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

#include "caf/actor_registry.hpp"

#include "caf/logger.hpp"
#include "caf/attachable.hpp"
#include "caf/actor_system.hpp"

namespace caf {

actor_registry::~actor_registry() {
  // nop
}

actor_registry::actor_registry(actor_system& sys) : running_(0), system_(sys) {
  // nop
}

strong_actor_ptr actor_registry::get(actor_id key) const {
  auto ptr = actor_id_cache::get(key);
  if (!ptr)
    CAF_LOG_DEBUG("key invalid, assume actor no longer exists:"
                  << CAF_ARG(key));
  return ptr;
}

void actor_registry::put(actor_id key, strong_actor_ptr val) {
  CAF_LOG_TRACE(CAF_ARG(key));
  if (!val)
    return;
  if (!actor_id_cache::put(key, val))
    return;
  CAF_LOG_INFO("added actor:" << CAF_ARG(key));
  actor_registry* reg = this;
  val->get()->attach_functor([key, reg]() {
    reg->erase(key);
  });
}

void actor_registry::erase(actor_id key) {
  actor_id_cache::erase(key);
}

strong_actor_ptr actor_registry::get(atom_value id) const {
  return atom_value_cache::get(id);
}

void actor_registry::put(atom_value key, strong_actor_ptr val) {
  if (val)
    val->get()->attach_functor([=] {
      system_.registry().put(key, nullptr);
    });
  atom_value_cache::put(key, std::move(val));
}

void actor_registry::erase(atom_value key) {
  atom_value_cache::erase(key);
}

void actor_registry::inc_running() {
# if defined(CAF_LOG_LEVEL) && CAF_LOG_LEVEL >= CAF_LOG_LEVEL_DEBUG
  auto value = ++running_;
  CAF_LOG_DEBUG(CAF_ARG(value));
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

void actor_registry::await_running_count_equal(size_t expected) const {
  CAF_ASSERT(expected == 0 || expected == 1);
  CAF_LOG_TRACE(CAF_ARG(expected));
  std::unique_lock<std::mutex> guard{running_mtx_};
  while (running_ != expected) {
    CAF_LOG_DEBUG(CAF_ARG(running_.load()));
    running_cv_.wait(guard);
  }
}

void actor_registry::start() {
  // nop
}

void actor_registry::stop() {
  // nop
}

} // namespace caf
