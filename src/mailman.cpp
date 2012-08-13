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

void mailman_loop() {
    bool done = false;
    // serializes outgoing messages
    binary_serializer bs;
    // connected tcp peers
    std::map<process_information, native_socket_type> peers;
    do_receive (
        on_arg_match >> [&](process_information_ptr target_peer, addressed_message msg) {
            auto i = peers.find(*target_peer);
            if (i != peers.end()) {
                bool disconnect_peer = false;
                auto peer_fd = i->second;
                try {
                    bs << msg;
                    DEBUG("--> " << to_string(msg));
                    auto sent = ::send(peer_fd, bs.sendable_data(), bs.sendable_size(), 0);
                    if (sent < 0 || static_cast<size_t>(sent) != bs.sendable_size()) {
                        disconnect_peer = true;
                        DEBUG("too few bytes written");
                    }
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
                DEBUG("message to an unknown peer: " << to_string(msg));
            }
        },
        on_arg_match >> [&](native_socket_type sockfd, process_information_ptr pinfo) {
            DEBUG("mailman: add_peer");
            auto i = peers.find(*pinfo);
            if (i == peers.end()) {
                //cout << "mailman added " << pjob.pinfo->process_id() << "@"
                //     << to_string(pjob.pinfo->node_id()) << endl;
                peers.insert(std::make_pair(*pinfo, sockfd));
            }
            else {
                DEBUG("add_peer_job failed: peer already known");
            }
        },
        on(atom("DONE")) >> [&]() {
            done = true;
        },
        others() >> [&]() {
            std::string str = "unexpected message in post_office: ";
            str += to_string(self->last_dequeued());
            CPPA_CRITICAL(str.c_str());
        }
    )
    .until(gref(done));
}

} } // namespace cppa::detail
