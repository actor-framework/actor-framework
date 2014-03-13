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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include <atomic>
#include <iostream>

#include "cppa/logging.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/exception.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/local_actor.hpp"

#include "cppa/io/middleman.hpp"

#include "cppa/detail/empty_tuple.hpp"
#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

#ifdef CPPA_OPENCL
#  include "cppa/opencl/opencl_metainfo.hpp"
#else
namespace cppa { namespace opencl {

class opencl_metainfo : public detail::singleton_mixin<opencl_metainfo> { };

} } // namespace cppa::opencl
#endif

namespace cppa { void shutdown() { detail::singleton_manager::shutdown(); } }

namespace cppa { namespace detail {

namespace {

std::atomic<opencl::opencl_metainfo*> s_opencl_metainfo;
std::atomic<uniform_type_info_map*> s_uniform_type_info_map;
std::atomic<io::middleman*> s_middleman;
std::atomic<actor_registry*> s_actor_registry;
std::atomic<group_manager*> s_group_manager;
std::atomic<empty_tuple*> s_empty_tuple;
std::atomic<scheduler::coordinator*> s_scheduling_coordinator;
std::atomic<logging*> s_logger;

} // namespace <anonymous>

void singleton_manager::shutdown() {
    CPPA_LOGF_DEBUG("shutdown scheduler");
    destroy(s_scheduling_coordinator);
    CPPA_LOGF_DEBUG("shutdown middleman");
    destroy(s_middleman);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    // it's safe to delete all other singletons now
    CPPA_LOGF_DEBUG("close OpenCL metainfo");
    destroy(s_opencl_metainfo);
    CPPA_LOGF_DEBUG("close actor registry");
    destroy(s_actor_registry);
    CPPA_LOGF_DEBUG("shutdown group manager");
    destroy(s_group_manager);
    CPPA_LOGF_DEBUG("destroy empty tuple singleton");
    destroy(s_empty_tuple);
    CPPA_LOGF_DEBUG("clear type info map");
    destroy(s_uniform_type_info_map);
    destroy(s_logger);
}

opencl::opencl_metainfo* singleton_manager::get_opencl_metainfo() {
#   ifdef CPPA_OPENCL
    return lazy_get(s_opencl_metainfo);
#   else
    CPPA_LOGF_ERROR("libcppa was compiled without OpenCL support");
    throw std::logic_error("libcppa was compiled without OpenCL support");
#   endif
}

actor_registry* singleton_manager::get_actor_registry() {

    return lazy_get(s_actor_registry);
}

uniform_type_info_map* singleton_manager::get_uniform_type_info_map() {
    return lazy_get(s_uniform_type_info_map);
}

group_manager* singleton_manager::get_group_manager() {
    return lazy_get(s_group_manager);
}

scheduler::coordinator* singleton_manager::get_scheduling_coordinator() {
    return lazy_get(s_scheduling_coordinator);
}

logging* singleton_manager::get_logger() {
    return lazy_get(s_logger);
}

io::middleman* singleton_manager::get_middleman() {
    return lazy_get(s_middleman);
}

empty_tuple* singleton_manager::get_empty_tuple() {
    return lazy_get(s_empty_tuple);
}

} } // namespace cppa::detail
