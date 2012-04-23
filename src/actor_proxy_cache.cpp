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


#include "cppa/atom.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/detail/thread.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

// thread_specific_ptr
#include <boost/thread/tss.hpp>

namespace {

boost::thread_specific_ptr<cppa::detail::actor_proxy_cache> s_proxy_cache;

} // namespace <anonmyous>

namespace cppa { namespace detail {

actor_proxy_cache& get_actor_proxy_cache()
{
    if (s_proxy_cache.get() == nullptr)
    {
        s_proxy_cache.reset(new actor_proxy_cache);
    }
    return *s_proxy_cache;
}

process_information_ptr
actor_proxy_cache::get_pinfo(const actor_proxy_cache::key_tuple& key)
{
    auto i = m_pinfos.find(key);
    if (i != m_pinfos.end())
    {
        return i->second;
    }
    process_information_ptr tmp(new process_information(std::get<1>(key),
                                                        std::get<2>(key)));
    m_pinfos.insert(std::make_pair(key, tmp));
    return tmp;
}

actor_proxy_ptr actor_proxy_cache::get(const key_tuple& key)
{
    auto i = m_proxies.find(key);
    if (i != m_proxies.end())
    {
        return i->second;
    }
    // get_pinfo(key) also inserts to m_pinfos
    actor_proxy_ptr result(new actor_proxy(std::get<0>(key), get_pinfo(key)));
    m_proxies.insert(std::make_pair(key, result));
    if (m_new_cb) m_new_cb(result);
    // insert to m_proxies
    //result->enqueue(message(result, nullptr, atom("MONITOR")));
    return result;
}

void actor_proxy_cache::add(actor_proxy_ptr& pptr)
{
    auto pinfo = pptr->parent_process_ptr();
    key_tuple key(pptr->id(), pinfo->process_id(), pinfo->node_id());
    m_pinfos.insert(std::make_pair(key, pptr->parent_process_ptr()));
    m_proxies.insert(std::make_pair(key, pptr));
    if (m_new_cb) m_new_cb(pptr);
}

void actor_proxy_cache::erase(const actor_proxy_ptr& pptr)
{
    auto pinfo = pptr->parent_process_ptr();
    key_tuple key(pptr->id(), pinfo->process_id(), pinfo->node_id());
    m_proxies.erase(key);
}

size_t actor_proxy_cache::size() const
{
    return m_proxies.size();
}

} } // namespace cppa::detail
