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

#include <atomic>

#include "caf/message.hpp"
#include "caf/exception.hpp"
#include "caf/scheduler.hpp"
#include "caf/local_actor.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/group_manager.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/uniform_type_info_map.hpp"

namespace caf {
namespace detail {

namespace {

std::atomic<abstract_singleton*> s_plugins[singletons::max_plugins];
std::mutex s_plugins_mtx;

std::atomic<scheduler::abstract_coordinator*> s_scheduling_coordinator;
std::mutex s_scheduling_coordinator_mtx;

std::atomic<uniform_type_info_map*> s_uniform_type_info_map;
std::mutex s_uniform_type_info_map_mtx;

std::atomic<actor_registry*> s_actor_registry;
std::mutex s_actor_registry_mtx;

std::atomic<group_manager*> s_group_manager;
std::mutex s_group_manager_mtx;

std::atomic<node_id::data*> s_node_id;
std::mutex s_node_id_mtx;

std::atomic<logging*> s_logger;
std::mutex s_logger_mtx;

} // namespace <anonymous>

abstract_singleton::~abstract_singleton() {
  // nop
}

std::mutex& singletons::get_plugin_mutex() {
  return s_plugins_mtx;
}

void singletons::stop_singletons() {
  // stop singletons, i.e., make sure no background threads/actors are running
  CAF_LOGF_DEBUG("stop plugins");
  for (auto& plugin : s_plugins) {
    stop(plugin);
  }
  CAF_LOGF_DEBUG("stop group manager");
  stop(s_group_manager);
  CAF_LOGF_DEBUG("stop scheduler");
  stop(s_scheduling_coordinator);
  CAF_LOGF_DEBUG("wait for all detached threads");
  scheduler::await_detached_threads();
  CAF_LOGF_DEBUG("stop actor registry");
  stop(s_actor_registry);
  // dispose singletons, i.e., release memory
  CAF_LOGF_DEBUG("dispose plugins");
  for (auto& plugin : s_plugins) {
    dispose(plugin);
  }
  CAF_LOGF_DEBUG("dispose group manager");
  dispose(s_group_manager);
  CAF_LOGF_DEBUG("dispose scheduler");
  dispose(s_scheduling_coordinator);
  CAF_LOGF_DEBUG("dispose registry");
  dispose(s_actor_registry);
  // final steps
  CAF_LOGF_DEBUG("stop and dispose logger, bye");
  stop(s_logger);
  stop(s_uniform_type_info_map);
  stop(s_node_id);
  dispose(s_logger);
  dispose(s_uniform_type_info_map);
  dispose(s_node_id);
}

actor_registry* singletons::get_actor_registry() {
  return lazy_get(s_actor_registry, s_actor_registry_mtx);
}

uniform_type_info_map* singletons::get_uniform_type_info_map() {
  return lazy_get(s_uniform_type_info_map, s_uniform_type_info_map_mtx);
}

group_manager* singletons::get_group_manager() {
  return lazy_get(s_group_manager, s_group_manager_mtx);
}

scheduler::abstract_coordinator* singletons::get_scheduling_coordinator() {
  return lazy_get(s_scheduling_coordinator, s_scheduling_coordinator_mtx);
}

bool singletons::set_scheduling_coordinator(scheduler::abstract_coordinator*p) {
  auto res = lazy_get(s_scheduling_coordinator, s_scheduling_coordinator_mtx,
                      [p] { return p; });
  return res == p;
}

node_id singletons::get_node_id() {
  return node_id{lazy_get(s_node_id, s_node_id_mtx)};
}

bool singletons::set_node_id(node_id::data* ptr) {
  auto res = lazy_get(s_node_id, s_node_id_mtx,
                      [ptr] { return ptr; });
  return res == ptr;
}

logging* singletons::get_logger() {
  return lazy_get(s_logger, s_logger_mtx);
}

std::atomic<abstract_singleton*>& singletons::get_plugin_singleton(size_t id) {
  CAF_ASSERT(id < max_plugins);
  return s_plugins[id];
}

} // namespace detail
} // namespace caf
