/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#include <atomic>
#include <iostream>

#include "cppa/message.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/exception.hpp"
#include "cppa/local_actor.hpp"

#include "cppa/detail/logging.hpp"
#include "cppa/detail/singletons.hpp"
#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

namespace cppa {
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
    CPPA_LOGF_DEBUG("stop scheduler");
    stop(s_scheduling_coordinator);
    CPPA_LOGF_DEBUG("stop plugins");
    for (auto& plugin : s_plugins) stop(plugin);
    CPPA_LOGF_DEBUG("stop actor registry");
    stop(s_actor_registry);
    CPPA_LOGF_DEBUG("stop group manager");
    stop(s_group_manager);
    CPPA_LOGF_DEBUG("stop type info map");
    stop(s_uniform_type_info_map);
    stop(s_logger);
    stop(s_node_id);
    // dispose singletons
    dispose(s_scheduling_coordinator);
    for (auto& plugin : s_plugins) dispose(plugin);
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

bool singletons::set_scheduling_coordinator(scheduler::abstract_coordinator*p) {
    scheduler::abstract_coordinator* expected = nullptr;
    return s_scheduling_coordinator.compare_exchange_weak(expected, p);
}

node_id singletons::get_node_id() {
    return node_id{lazy_get(s_node_id)};
}

logging* singletons::get_logger() { return lazy_get(s_logger); }

std::atomic<abstract_singleton*>& singletons::get_plugin_singleton(size_t id) {
    CPPA_REQUIRE(id < max_plugin_singletons);
    return s_plugins[id];
}

} // namespace detail
} // namespace cppa
