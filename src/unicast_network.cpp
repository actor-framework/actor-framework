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
#include "cppa/exit_reason.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/network/middleman.hpp"
#include "cppa/network/ipv4_acceptor.hpp"
#include "cppa/network/ipv4_io_stream.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

using namespace detail;
using namespace network;

void publish(actor_ptr whom, std::unique_ptr<acceptor> acceptor) {
    if (!whom && !acceptor) return;
    singleton_manager::get_actor_registry()->put(whom->id(), whom);
    singleton_manager::get_middleman()->publish(std::move(acceptor), whom);
}

actor_ptr remote_actor(io_stream_ptr_pair peer) {
    auto pinf = process_information::get();
    std::uint32_t process_id = pinf->process_id();
    // throws on error
    peer.second->write(&process_id, sizeof(std::uint32_t));
    peer.second->write(pinf->node_id().data(), pinf->node_id().size());
    std::uint32_t remote_actor_id;
    std::uint32_t peer_pid;
    process_information::node_id_type peer_node_id;
    peer.first->read(&remote_actor_id, sizeof(remote_actor_id));
    peer.first->read(&peer_pid, sizeof(std::uint32_t));
    peer.first->read(peer_node_id.data(), peer_node_id.size());
    process_information_ptr pinfptr(new process_information(peer_pid, peer_node_id));
    if (*pinf == *pinfptr) {
        // dude, this is not a remote actor, it's a local actor!
#       ifdef CPPA_DEBUG
        std::cerr << "*** warning: remote_actor() called to access a local actor\n"
                  << std::flush;
#       endif
        return singleton_manager::get_actor_registry()->get(remote_actor_id);
    }
    //auto key = std::make_tuple(remote_actor_id, pinfptr->process_id(), pinfptr->node_id());
    singleton_manager::get_middleman()->add_peer(peer, pinfptr);
    return get_actor_proxy_cache().get_or_put(remote_actor_id,
                                              pinfptr->process_id(),
                                              pinfptr->node_id());
}

void publish(actor_ptr whom, std::uint16_t port, const char* addr) {
    if (whom) publish(whom, ipv4_acceptor::create(port, addr));
}

actor_ptr remote_actor(const char* host, std::uint16_t port) {
    // throws on error
    io_stream_ptr peer = ipv4_io_stream::connect_to(host, port);
    io_stream_ptr_pair ptrpair(peer, peer);
    return remote_actor(ptrpair);
}

} // namespace cppa
