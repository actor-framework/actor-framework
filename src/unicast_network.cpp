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


#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <cstring>    // memset
#include <iostream>
#include <stdexcept>

#include <netinet/tcp.h>

#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/singletons.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"
#include "cppa/intrusive/blocking_single_reader_queue.hpp"

#include "cppa/io/acceptor.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/peer_acceptor.hpp"
#include "cppa/io/ipv4_acceptor.hpp"
#include "cppa/io/ipv4_io_stream.hpp"
#include "cppa/io/remote_actor_proxy.hpp"

namespace cppa {

using namespace detail;
using namespace io;

void publish(actor_ptr whom, std::unique_ptr<acceptor> aptr) {
    CPPA_LOG_TRACE(CPPA_TARG(whom, to_string) << ", " << CPPA_MARG(ptr, get)
                   << ", args.size() = " << args.size());
    if (!whom) return;
    CPPA_REQUIRE(args.size() == 0);
    get_actor_registry()->put(whom->id(), whom);
    auto mm = get_middleman();
    mm->register_acceptor(whom, new peer_acceptor(mm, move(aptr), whom));
}

void publish(actor_ptr whom, std::uint16_t port, const char* addr) {
    if (!whom) return;
    publish(whom, ipv4_acceptor::create(port, addr));
}

actor_ptr remote_actor(stream_ptr_pair io) {
    CPPA_LOG_TRACE("io{" << io.first.get() << ", " << io.second.get() << "}");
    auto pinf = node_id::get();
    std::uint32_t process_id = pinf->process_id();
    // throws on error
    io.second->write(&process_id, sizeof(std::uint32_t));
    io.second->write(pinf->host_id().data(), pinf->host_id().size());
    actor_id remote_aid;
    std::uint32_t peer_pid;
    node_id::host_id_type peer_node_id;
    io.first->read(&remote_aid, sizeof(actor_id));
    io.first->read(&peer_pid, sizeof(std::uint32_t));
    io.first->read(peer_node_id.data(), peer_node_id.size());
    auto pinfptr = make_counted<node_id>(peer_pid, peer_node_id);
    if (*pinf == *pinfptr) {
        // this is a local actor, not a remote actor
        CPPA_LOGF_WARNING("remote_actor() called to access a local actor");
        return get_actor_registry()->get(remote_aid);
    }
    auto mm = get_middleman();
    struct remote_actor_result { remote_actor_result* next; actor_ptr value; };
    intrusive::blocking_single_reader_queue<remote_actor_result> q;
    mm->run_later([mm, io, pinfptr, remote_aid, &q] {
        CPPA_LOGC_TRACE("cppa",
                        "remote_actor$create_connection", "");
        auto pp = mm->get_peer(*pinfptr);
        CPPA_LOGF_INFO_IF(pp, "connection already exists (re-use old one)");
        if (!pp) mm->new_peer(io.first, io.second, pinfptr);
        auto res = mm->get_namespace().get_or_put(pinfptr, remote_aid);
        q.push_back(new remote_actor_result{0, res});
    });
    std::unique_ptr<remote_actor_result> result(q.pop());
    CPPA_LOGF_DEBUG("result = " << result->value.get());
    return result->value;
}
    
actor_ptr remote_actor(const char* host, std::uint16_t port) {
    auto io = ipv4_io_stream::connect_to(host, port);
    return remote_actor(stream_ptr_pair(io, io));
}

} // namespace cppa
