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


#include <utility>

#include "cppa/logging.hpp"
#include "cppa/node_id.hpp"
#include "cppa/serializer.hpp"
#include "cppa/singletons.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/actor_namespace.hpp"

#include "cppa/io/middleman.hpp"
#include "cppa/io/remote_actor_proxy.hpp"

#include "cppa/detail/raw_access.hpp"
#include "cppa/detail/actor_registry.hpp"

namespace cppa {

void actor_namespace::write(serializer* sink, const actor_addr& addr) {
    CPPA_REQUIRE(sink != nullptr);
    if (!addr) {
        node_id::host_id_type zero;
        std::fill(zero.begin(), zero.end(), 0);
        sink->write_value(static_cast<uint32_t>(0));         // actor id
        sink->write_value(static_cast<uint32_t>(0));         // process id
        sink->write_raw(node_id::host_id_size, zero.data()); // host id
    }
    else {
        // register locally running actors to be able to deserialize them later
        if (!addr.is_remote()) {
            get_actor_registry()->put(addr.id(), detail::raw_access::get(addr));
        }
        auto& pinf = addr.node();
        sink->write_value(addr.id());                                  // actor id
        sink->write_value(pinf.process_id());                          // process id
        sink->write_raw(node_id::host_id_size, pinf.host_id().data()); // host id
    }
}

actor_addr actor_namespace::read(deserializer* source) {
    CPPA_REQUIRE(source != nullptr);
    node_id::host_id_type hid;
    auto aid = source->read<uint32_t>();                 // actor id
    auto pid = source->read<uint32_t>();                 // process id
    source->read_raw(node_id::host_id_size, hid.data()); // host id
    auto this_node = node_id::get();
    if (aid == 0 && pid == 0) {
        // 0:0 identifies an invalid actor
        return invalid_actor_addr;
    }
    else if (pid == this_node->process_id() && hid == this_node->host_id()) {
        // identifies this exact process on this host, ergo: local actor
        return get_actor_registry()->get(aid)->address();
    }
    else {
        // identifies a remote actor; create proxy if needed
        node_id_ptr tmp = new node_id{pid, hid};
        return get_or_put(tmp, aid)->address();
    }
}

size_t actor_namespace::count_proxies(const node_id& node) {
    auto i = m_proxies.find(node);
    return (i != m_proxies.end()) ? i->second.size() : 0;
}

actor_proxy_ptr actor_namespace::get(const node_id& node, actor_id aid) {
    auto& submap = m_proxies[node];
    auto i = submap.find(aid);
    if (i != submap.end()) {
        auto result = i->second.promote();
        CPPA_LOG_INFO_IF(!result, "proxy instance expired; "
                         << CPPA_TARG(node, to_string)
                         << ", "<< CPPA_ARG(aid));
        if (!result) submap.erase(i);
        return result;
    }
    return nullptr;
}

actor_proxy_ptr actor_namespace::get_or_put(node_id_ptr node, actor_id aid) {
    auto result = get(*node, aid);
    if (result == nullptr && m_factory) {
        auto ptr = m_factory(aid, node);
        put(*node, aid, ptr);
        result = ptr;
    }
    return result;
}

void actor_namespace::put(const node_id& node,
                          actor_id aid,
                          const actor_proxy_ptr& proxy) {
    auto& submap = m_proxies[node];
    auto i = submap.find(aid);
    if (i == submap.end()) {
        submap.insert(std::make_pair(aid, proxy));
        if (m_new_element_callback) m_new_element_callback(aid, node);
    }
    else {
        CPPA_LOG_ERROR("proxy for " << aid << ":"
                       << to_string(node) << " already exists");
    }
}

auto actor_namespace::proxies(node_id& node) -> proxy_map& {
    return m_proxies[node];
}

void actor_namespace::erase(node_id& inf) {
    CPPA_LOG_TRACE(CPPA_TARG(inf, to_string));
    m_proxies.erase(inf);
}

void actor_namespace::erase(node_id& inf, actor_id aid) {
    CPPA_LOG_TRACE(CPPA_TARG(inf, to_string) << ", " << CPPA_ARG(aid));
    auto i = m_proxies.find(inf);
    if (i != m_proxies.end()) {
        i->second.erase(aid);
    }
}

} // namespace cppa
