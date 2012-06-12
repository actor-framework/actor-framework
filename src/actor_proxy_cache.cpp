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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
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


#include <thread>
#include <cstring>

#include "cppa/atom.hpp"
#include "cppa/any_tuple.hpp"

#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"
#include "cppa/detail/singleton_manager.hpp"

// thread_specific_ptr
//#include <boost/thread/tss.hpp>

namespace {

//boost::thread_specific_ptr<cppa::detail::actor_proxy_cache> s_proxy_cache;

cppa::detail::actor_proxy_cache s_proxy_cache;

} // namespace <anonmyous>

namespace cppa { namespace detail {

actor_proxy_cache& get_actor_proxy_cache() {
    /*
    if (s_proxy_cache.get() == nullptr) {
        s_proxy_cache.reset(new actor_proxy_cache);
    }
    return *s_proxy_cache;
    */
    return s_proxy_cache;
}

actor_proxy_ptr actor_proxy_cache::get(actor_id aid,
                                       std::uint32_t process_id,
                                       const process_information::node_id_type& node_id) {
    key_tuple k{node_id, process_id, aid};
    return get_impl(k);
}

actor_proxy_ptr actor_proxy_cache::get_impl(const key_tuple& key) {
    { // lifetime scope of shared guard
        util::shared_lock_guard<util::shared_spinlock> guard{m_lock};
        auto i = m_entries.find(key);
        if (i != m_entries.end()) {
            return i->second;
        }
    }
    actor_proxy_ptr result{new actor_proxy(std::get<2>(key), new process_information(std::get<1>(key), std::get<0>(key)))};
    { // lifetime scope of exclusive guard
        std::lock_guard<util::shared_spinlock> guard{m_lock};
        auto i = m_entries.find(key);
        if (i != m_entries.end()) {
            return i->second;
        }
        m_entries.insert(std::make_pair(key, result));
    }
    result->attach_functor([result](std::uint32_t) {
        get_actor_proxy_cache().erase(result);
    });
    result->enqueue(nullptr, make_any_tuple(atom("MONITOR")));
    return result;
}

bool actor_proxy_cache::erase(const actor_proxy_ptr& pptr) {
    auto pinfo = pptr->parent_process_ptr();
    key_tuple key(pinfo->node_id(), pinfo->process_id(), pptr->id()); {
        std::lock_guard<util::shared_spinlock> guard{m_lock};
        return m_entries.erase(key) > 0;
    }
    return false;
}

bool actor_proxy_cache::key_tuple_less::operator()(const key_tuple& lhs,
                                                   const key_tuple& rhs) const {
    int cmp_res = strncmp(reinterpret_cast<const char*>(std::get<0>(lhs).data()),
                          reinterpret_cast<const char*>(std::get<0>(rhs).data()),
                          process_information::node_id_size);
    if (cmp_res < 0) {
        return true;
    }
    else if (cmp_res == 0) {
        if (std::get<1>(lhs) < std::get<1>(rhs)) {
            return true;
        }
        else if (std::get<1>(lhs) == std::get<1>(rhs)) {
            return std::get<2>(lhs) < std::get<2>(rhs);
        }
    }
    return false;
}


} } // namespace cppa::detail
