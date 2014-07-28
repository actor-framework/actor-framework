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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <atomic>

#include "caf/message.hpp"
#include "caf/scheduler.hpp"
#include "caf/exception.hpp"
#include "caf/local_actor.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/group_manager.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/uniform_type_info_map.hpp"

namespace caf {
namespace detail {

namespace {

std::atomic<abstract_singleton*> s_plugins[singletons::max_plugin_singletons];
std::atomic<scheduler::abstract_coordinator*> s_scheduling_coordinator;
std::atomic<uniform_type_info_map*> s_uniform_type_info_map;
std::atomic<actor_registry*> s_actor_registry;
std::atomic<group_manager*> s_group_manager;
std::atomic<node_id::data*> s_node_id;
std::atomic<logging*> s_logger;

} // namespace <anonymous>

abstract_singleton::~abstract_singleton() {
  // nop
}

void singletons::stop_singletons() {
  CAF_LOGF_DEBUG("stop scheduler");
  stop(s_scheduling_coordinator);
  CAF_LOGF_DEBUG("stop plugins");
  for (auto& plugin : s_plugins) {
    stop(plugin);
  }
  CAF_LOGF_DEBUG("stop actor registry");
  stop(s_actor_registry);
  CAF_LOGF_DEBUG("stop group manager");
  stop(s_group_manager);
  CAF_LOGF_DEBUG("stop type info map");
  stop(s_uniform_type_info_map);
  stop(s_logger);
  stop(s_node_id);
  // dispose singletons
  dispose(s_scheduling_coordinator);
  for (auto& plugin : s_plugins) {
    dispose(plugin);
  }
  dispose(s_actor_registry);
  dispose(s_group_manager);
  dispose(s_uniform_type_info_map);
  dispose(s_logger);
  dispose(s_node_id);
}

actor_registry* singletons::get_actor_registry() {
  return lazy_get(s_actor_registry);
}

uniform_type_info_map* singletons::get_uniform_type_info_map() {
  return lazy_get(s_uniform_type_info_map);
}

group_manager* singletons::get_group_manager() {
  return lazy_get(s_group_manager);
}

scheduler::abstract_coordinator* singletons::get_scheduling_coordinator() {
  return lazy_get(s_scheduling_coordinator);
}

bool singletons::set_scheduling_coordinator(scheduler::abstract_coordinator* p) {
  scheduler::abstract_coordinator* expected = nullptr;
  return s_scheduling_coordinator.compare_exchange_weak(expected, p);
}

node_id singletons::get_node_id() {
  return node_id{lazy_get(s_node_id)};
}

logging* singletons::get_logger() {
  return lazy_get(s_logger);
}

std::atomic<abstract_singleton*>& singletons::get_plugin_singleton(size_t id) {
  CAF_REQUIRE(id < max_plugin_singletons);
  return s_plugins[id];
}

} // namespace detail
} // namespace caf
