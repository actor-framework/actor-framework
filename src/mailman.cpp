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


#include <atomic>
#include <iostream>

#ifdef CPPA_WINDOWS
#else
#   include <netdb.h>
#   include <unistd.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <fcntl.h>
#endif

#include "cppa/cppa.hpp"
#include "cppa/to_string.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/detail/post_office.hpp"

#define DEBUG(arg)                                                             \
    std::cout << "[process id: "                                               \
              << cppa::process_information::get()->process_id()                \
              << "] " << arg << std::endl

#undef DEBUG
#define DEBUG(unused) ((void) 0)

using std::cout;
using std::cerr;
using std::endl;

// implementation of mailman.hpp
namespace cppa { namespace detail {

namespace {

template<typename T, typename... Args>
void call_ctor(T& var, Args&&... args) {
    new (&var) T (std::forward<Args>(args)...);
}

template<typename T>
void call_dtor(T& var) {
    var.~T();
}

} // namespace <anonymous>

mm_message::mm_message() : next(0), type(mm_message_type::shutdown) { }

mm_message::mm_message(process_information_ptr a0, addressed_message a1)
: next(0), type(mm_message_type::outgoing_message) {
    call_ctor(out_msg, std::move(a0), std::move(a1));
}

mm_message::mm_message(util::io_stream_ptr_pair a0, process_information_ptr a1)
: next(0), type(mm_message_type::add_peer) {
    call_ctor(peer, std::move(a0), std::move(a1));
}

mm_message::~mm_message() {
    switch (type) {
        case mm_message_type::outgoing_message: {
            call_dtor(out_msg);
            break;
        }
        case mm_message_type::add_peer: {
            call_dtor(peer);
            break;
        }
        default: break;
    }
}

void mailman_loop(intrusive::single_reader_queue<mm_message>& q) {
    bool done = false;
    // serializes outgoing messages
    binary_serializer bs;
    // connected tcp peers
    std::map<process_information, util::io_stream_ptr_pair> peers;
    std::unique_ptr<mm_message> msg;
    auto fetch_next = [&] { msg.reset(q.pop()); };
    for (fetch_next(); !done; fetch_next()) {
        switch (msg->type) {
            case mm_message_type::outgoing_message: {
                auto& target_peer = msg->out_msg.first;
                auto& out_msg = msg->out_msg.second;
                CPPA_REQUIRE(target_peer != nullptr);
                auto i = peers.find(*target_peer);
                if (i != peers.end()) {
                    bool disconnect_peer = false;
                    try {
                        bs << out_msg;
                        DEBUG("--> " << to_string(out_msg));
                        DEBUG("outgoing message size: " << bs.size());
                        i->second.second->write(bs.sendable_data(),
                                                bs.sendable_size());
                    }
                    // something went wrong; close connection to this peer
                    catch (std::exception& e) {
                        DEBUG(to_uniform_name(typeid(e)) << ": " << e.what());
                        disconnect_peer = true;
                    }
                    if (disconnect_peer) {
                        DEBUG("peer disconnected (error during send)");
                        //closesocket(peer);
                        //post_office_close_socket(peer_fd);
                        peers.erase(i);
                    }
                    bs.reset();
                }
                else {
                    DEBUG("message to an unknown peer: " << to_string(out_msg));
                }
                break;
            }
            case mm_message_type::add_peer: {
                DEBUG("mailman: add_peer");
                auto& iopair = msg->peer.first;
                auto& pinfo = msg->peer.second;
                auto i = peers.find(*pinfo);
                if (i == peers.end()) {
                    //cout << "mailman added " << pjob.pinfo->process_id() << "@"
                    //     << to_string(pjob.pinfo->node_id()) << endl;
                    peers.insert(std::make_pair(*pinfo, iopair));
                }
                else {
                    DEBUG("add_peer failed: peer already known");
                }
                break;
            }
            case mm_message_type::shutdown: {
                done = true;
            }
        }
    }
}

} } // namespace cppa::detail
