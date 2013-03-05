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


#include <cstdint>

#include "cppa/logging.hpp"
#include "cppa/to_string.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/primitive_variant.hpp"

#include "cppa/network/default_actor_proxy.hpp"
#include "cppa/network/default_actor_addressing.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

using namespace std;

namespace cppa { namespace network {

default_actor_addressing::default_actor_addressing(default_protocol* parent)
: m_parent(parent), m_pinf(process_information::get()) { }

atom_value default_actor_addressing::technology_id() const {
    return atom("DEFAULT");
}

void default_actor_addressing::write(serializer* sink, const actor_ptr& ptr) {
    CPPA_REQUIRE(sink != nullptr);
    if (ptr == nullptr) {
        CPPA_LOG_DEBUG("serialized nullptr");
        sink->begin_object("@0");
        sink->end_object();
    }
    else {
        // local actor?
        if (!ptr->is_proxy()) {
            get_actor_registry()->put(ptr->id(), ptr);
        }
        auto pinf = m_pinf;
        if (ptr->is_proxy()) {
            auto dptr = ptr.downcast<default_actor_proxy>();
            if (dptr) pinf = dptr->process_info();
            else {
                CPPA_LOG_ERROR("ptr is not a default_actor_proxy instance");
            }
        }
        sink->begin_object("@actor");
        sink->write_value(ptr->id());
        sink->write_value(pinf->process_id());
        sink->write_raw(process_information::node_id_size,
                        pinf->node_id().data());
        sink->end_object();
    }
}

actor_ptr default_actor_addressing::read(deserializer* source) {
    CPPA_REQUIRE(source != nullptr);
    auto cname = source->seek_object();
    if (cname == "@0") {
        CPPA_LOG_DEBUG("deserialized nullptr");
        source->begin_object("@0");
        source->end_object();
        return nullptr;
    }
    else if (cname == "@actor") {
        process_information::node_id_type nid;
        source->begin_object(cname);
        auto aid = source->read<uint32_t>();
        auto pid = source->read<uint32_t>();
        source->read_raw(process_information::node_id_size, nid.data());
        source->end_object();
        // local actor?
        auto pinf = process_information::get();
        if (pid == pinf->process_id() && nid == pinf->node_id()) {
            return get_actor_registry()->get(aid);
        }
        else {
            process_information tmp(pid, nid);
            return get_or_put(tmp, aid);
        }
    }
    else throw runtime_error("expected type name \"@0\" or \"@actor\"; "
                             "found: " + cname);
}

size_t default_actor_addressing::count_proxies(const process_information& inf) {
    auto i = m_proxies.find(inf);
    return (i != m_proxies.end()) ? i->second.size() : 0;
}

actor_ptr default_actor_addressing::get(const process_information& inf,
                                        actor_id aid) {
    auto& submap = m_proxies[inf];
    auto i = submap.find(aid);
    if (i != submap.end()) {
        auto result = i->second.promote();
        CPPA_LOG_INFO_IF(!result, "proxy instance expired; "
                                  << CPPA_TARG(inf, to_string) << ", "
                                  << CPPA_ARG(aid));
        return result;
    }
    return nullptr;
}

void default_actor_addressing::put(const process_information& node,
                                   actor_id aid,
                                   const actor_proxy_ptr& proxy) {
    auto& submap = m_proxies[node];
    auto i = submap.find(aid);
    if (i == submap.end()) {
        submap.insert(make_pair(aid, proxy));
        m_parent->enqueue(node,
                          {nullptr, nullptr},
                          make_any_tuple(atom("MONITOR"),
                                         process_information::get(),
                                         aid));
    }
    else {
        CPPA_LOG_ERROR("a proxy for " << aid << ":" << to_string(node)
                       << " already exists");
    }
}


actor_ptr default_actor_addressing::get_or_put(const process_information& inf,
                                               actor_id aid) {
    auto result = get(inf, aid);
    if (result == nullptr) {
        CPPA_LOG_INFO("created new proxy instance; "
                      << CPPA_TARG(inf, to_string) << ", " << CPPA_ARG(aid));
        auto ptr = make_counted<default_actor_proxy>(aid, new process_information(inf), m_parent);
        put(inf, aid, ptr);
        result = ptr;
    }
    return result;
}

auto default_actor_addressing::proxies(process_information& i) -> proxy_map& {
    return m_proxies[i];
}

void default_actor_addressing::erase(process_information& inf) {
    CPPA_LOG_TRACE("inf = " << to_string(inf));
    m_proxies.erase(inf);
}

void default_actor_addressing::erase(process_information& inf, actor_id aid) {
    CPPA_LOG_TRACE("inf = " << to_string(inf) << ", aid = " << aid);
    auto i = m_proxies.find(inf);
    if (i != m_proxies.end()) {
        i->second.erase(aid);
    }
}

} } // namespace cppa::network
