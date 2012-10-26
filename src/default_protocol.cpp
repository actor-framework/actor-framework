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


#include <future>
#include <cstdint>
#include <iostream>

#include "cppa/to_string.hpp"

#include "cppa/network/middleman.hpp"
#include "cppa/network/default_peer.hpp"
#include "cppa/network/ipv4_acceptor.hpp"
#include "cppa/network/ipv4_io_stream.hpp"
#include "cppa/network/default_protocol.hpp"
#include "cppa/network/default_peer_acceptor.hpp"

#include "cppa/detail/logging.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

using namespace std;
using namespace cppa::detail;

namespace cppa { namespace network {

default_protocol::default_protocol(abstract_middleman* parent)
: super(parent), m_addressing(this) { }

atom_value default_protocol::identifier() const {
    return atom("DEFAULT");
}

void default_protocol::publish(const actor_ptr& whom, variant_args args) {
    CPPA_LOG_TRACE(CPPA_TARG(whom, to_string)
                   << ", args.size() = " << args.size());
    if (!whom) return;
    CPPA_REQUIRE(args.size() == 2 || args.size() == 1);
    auto i = args.begin();
    if (args.size() == 1) {
        auto port = get<uint16_t>(*i);
        CPPA_LOG_INFO("publish " << to_string(whom) << " on port " << port);
        publish(whom, ipv4_acceptor::create(port), {});
    }
    else if (args.size() == 2) {
        auto port = get<uint16_t>(*i++);
        auto& addr = get<string>(*i);
        CPPA_LOG_INFO("publish " << to_string(whom) << " on port " << port
                      << " with addr = " << addr);
        publish(whom, ipv4_acceptor::create(port, addr.c_str()), {});
    }
    else throw logic_error("wrong number of arguments, expected one or two");
}

void default_protocol::publish(const actor_ptr& whom,
                               std::unique_ptr<acceptor> ptr,
                               variant_args args             ) {
    CPPA_LOG_TRACE(CPPA_TARG(whom, to_string) << ", " << CPPA_MARG(ptr, get)
                   << ", args.size() = " << args.size());
    if (!whom) return;
    CPPA_REQUIRE(args.size() == 0);
    static_cast<void>(args); // keep compiler happy
    singleton_manager::get_actor_registry()->put(whom->id(), whom);
    default_protocol_ptr proto = this;
    auto impl = make_counted<default_peer_acceptor>(this, move(ptr), whom);
    run_later([=] {
        CPPA_LOGF_TRACE("lambda from default_protocol::publish");
        proto->m_acceptors[whom].push_back(impl);
        proto->continue_reader(impl.get());
    });
}

void default_protocol::unpublish(const actor_ptr& whom) {
    CPPA_LOG_TRACE("whom = " << to_string(whom));
    default_protocol_ptr proto = this;
    run_later([=] {
        CPPA_LOGF_TRACE("lambda from default_protocol::unpublish");
        auto& acceptors = m_acceptors[whom];
        for (auto& ptr : acceptors) {
            proto->stop_reader(ptr.get());
        }
        m_acceptors.erase(whom);
    });
}

void default_protocol::register_peer(const process_information& node,
                                     default_peer* ptr) {
    CPPA_LOG_TRACE("node = " << to_string(node) << ", ptr = " << ptr);
    auto& ptrref = m_peers[node];
    if (ptrref) {
        CPPA_LOG_INFO("peer " << to_string(node) << " already defined");
    }
    else ptrref = ptr;
}

default_peer_ptr default_protocol::get_peer(const process_information& n) {
    CPPA_LOG_TRACE("n = " << to_string(n));
    auto e = end(m_peers);
    auto i = m_peers.find(n);
    if (i != e) {
        CPPA_LOG_DEBUG("result = " << i->second.get());
        return i->second;
    }
    CPPA_LOG_DEBUG("result = nullptr");
    return nullptr;
}

actor_ptr default_protocol::remote_actor(variant_args args) {
    CPPA_LOG_TRACE("args.size() = " << args.size());
    CPPA_REQUIRE(args.size() == 2);
    auto i = args.begin();
    auto port = get<uint16_t>(*i++);
    auto& host = get<string>(*i);
    auto io = ipv4_io_stream::connect_to(host.c_str(), port);
    return remote_actor(io_stream_ptr_pair(io, io), {});
}

struct remote_actor_result { remote_actor_result* next; actor_ptr value; };

actor_ptr default_protocol::remote_actor(io_stream_ptr_pair io,
                                         variant_args args         ) {
    CPPA_LOG_TRACE("io = {" << io.first.get() << ", " << io.second.get() << "}, "
                   << " << args.size() = " << args.size());
    CPPA_REQUIRE(args.size() == 0);
    auto pinf = process_information::get();
    std::uint32_t process_id = pinf->process_id();
    // throws on error
    io.second->write(&process_id, sizeof(std::uint32_t));
    io.second->write(pinf->node_id().data(), pinf->node_id().size());
    actor_id remote_aid;
    std::uint32_t peer_pid;
    process_information::node_id_type peer_node_id;
    io.first->read(&remote_aid, sizeof(actor_id));
    io.first->read(&peer_pid, sizeof(std::uint32_t));
    io.first->read(peer_node_id.data(), peer_node_id.size());
    auto pinfptr = make_counted<process_information>(peer_pid, peer_node_id);
    if (*pinf == *pinfptr) {
        // dude, this is not a remote actor, it's a local actor!
        CPPA_LOG_ERROR("remote_actor() called to access a local actor");
#       ifndef CPPA_DEBUG
        std::cerr << "*** warning: remote_actor() called to access a local actor\n"
                  << std::flush;
#       endif
        return singleton_manager::get_actor_registry()->get(remote_aid);
    }
    default_protocol_ptr proto = this;
    intrusive::single_reader_queue<remote_actor_result> q;
    run_later([proto, io, pinfptr, remote_aid, &q] {
        CPPA_LOGF_TRACE("lambda from default_protocol::remote_actor");
        auto pp = proto->get_peer(*pinfptr);
        CPPA_LOGF_INFO_IF(pp, "connection already exists (re-use old one)");
        if (!pp) proto->new_peer(io.first, io.second, pinfptr);
        auto res = proto->addressing()->get_or_put(*pinfptr, remote_aid);
        q.push_back(new remote_actor_result{0, res});
    });
    unique_ptr<remote_actor_result> result(q.pop());
    CPPA_LOGF_DEBUG("result = " << result->value.get());
    return result->value;
}

void default_protocol::erase_peer(const default_peer_ptr& pptr) {
    CPPA_REQUIRE(pptr != nullptr);
    CPPA_LOG_TRACE("pptr = " << pptr.get()
                   << ", pptr->node() = " << to_string(pptr->node()));
    stop_reader(pptr.get());
    auto i = m_peers.find(pptr->node());
    if (i != m_peers.end()) {
        CPPA_LOG_DEBUG_IF(i->second != pptr, "node " << to_string(pptr->node())
                                             << " does not exist in m_peers");
        if (i->second == pptr) {
            m_peers.erase(i);
        }
    }
}

void default_protocol::new_peer(const input_stream_ptr& in,
                                const output_stream_ptr& out,
                                const process_information_ptr& node) {
    CPPA_LOG_TRACE("");
    auto ptr = make_counted<default_peer>(this, in, out, node);
    continue_reader(ptr.get());
    if (node) register_peer(*node, ptr.get());
}

void default_protocol::continue_writer(const default_peer_ptr& pptr) {
    CPPA_LOG_TRACE(CPPA_MARG(pptr, get));
    super::continue_writer(pptr.get());
}

default_actor_addressing* default_protocol::addressing() {
    return &m_addressing;
}

} } // namespace cppa::network
