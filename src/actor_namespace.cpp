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

#include "cppa/detail/actor_registry.hpp"

namespace cppa {

void actor_namespace::write(serializer* sink, const actor_ptr& ptr) {
    CPPA_REQUIRE(sink != nullptr);
    if (ptr == nullptr) {
        CPPA_LOG_DEBUG("serialize nullptr");
        sink->write_value(static_cast<actor_id>(0));
        node_id::serialize_invalid(sink);
    }
    else {
        // local actor?
        if (!ptr->is_proxy()) {
            get_actor_registry()->put(ptr->id(), ptr);
        }
        auto pinf = node_id::get();
        if (ptr->is_proxy()) {
            auto dptr = ptr.downcast<io::remote_actor_proxy>();
            if (dptr) pinf = dptr->process_info();
            else CPPA_LOG_ERROR("downcast failed");
        }
        sink->write_value(ptr->id());
        sink->write_value(pinf->process_id());
        sink->write_raw(node_id::host_id_size,
                        pinf->host_id().data());
    }
}
    
actor_ptr actor_namespace::read(deserializer* source) {
    CPPA_REQUIRE(source != nullptr);
    node_id::host_id_type nid;
    auto aid = source->read<uint32_t>();
    auto pid = source->read<uint32_t>();
    source->read_raw(node_id::host_id_size, nid.data());
    // local actor?
    auto pinf = node_id::get();
    if (aid == 0 && pid == 0) {
        return nullptr;
    }
    else if (pid == pinf->process_id() && nid == pinf->host_id()) {
        return get_actor_registry()->get(aid);
    }
    else {
        node_id_ptr tmp = new node_id{pid, nid};
        return get_or_put(tmp, aid);
    }
    return nullptr;
}

size_t actor_namespace::count_proxies(const node_id& node) {
    auto i = m_proxies.find(node);
    return (i != m_proxies.end()) ? i->second.size() : 0;
}

actor_ptr actor_namespace::get(const node_id& node, actor_id aid) {
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
    
actor_ptr actor_namespace::get_or_put(node_id_ptr node, actor_id aid) {
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
        /*if (m_parent) {
            m_parent->enqueue(node,
                              {nullptr, nullptr},
                              make_any_tuple(atom("MONITOR"),
                                             node_id::get(),
                                             aid));
        }*/
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
    CPPA_LOGMF(CPPA_TRACE, self, CPPA_TARG(inf, to_string));
    m_proxies.erase(inf);
}

void actor_namespace::erase(node_id& inf, actor_id aid) {
    CPPA_LOGMF(CPPA_TRACE, self, CPPA_TARG(inf, to_string) << ", " << CPPA_ARG(aid));
    auto i = m_proxies.find(inf);
    if (i != m_proxies.end()) {
        i->second.erase(aid);
    }
}
    
} // namespace cppa
