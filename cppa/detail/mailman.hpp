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


#ifndef CPPA_MAILMAN_HPP
#define CPPA_MAILMAN_HPP

#include "cppa/any_tuple.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/acceptor.hpp"

#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/addressed_message.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace detail {

enum class mm_message_type {
    outgoing_message,
    add_peer,
    shutdown
};

struct mm_message {
    mm_message* next;
    mm_message_type type;
    union {
        std::pair<process_information_ptr, addressed_message> out_msg;
        std::pair<util::io_stream_ptr_pair, process_information_ptr> peer;
    };
    mm_message();
    mm_message(process_information_ptr, addressed_message);
    mm_message(util::io_stream_ptr_pair, process_information_ptr);
    ~mm_message();
    template<typename... Args>
    static inline std::unique_ptr<mm_message> create(Args&&... args) {
        return std::unique_ptr<mm_message>(new mm_message(std::forward<Args>(args)...));
    }
};

void mailman_loop(intrusive::single_reader_queue<mm_message>& q);

template<typename... Args>
inline void send2mm(Args&&... args) {
    auto nm = singleton_manager::get_network_manager();
    nm->send_to_mailman(mm_message::create(std::forward<Args>(args)...));
}

inline void mailman_enqueue(process_information_ptr peer,
                            addressed_message outgoing_message) {
    send2mm(std::move(peer), std::move(outgoing_message));
}

inline void mailman_add_peer(util::io_stream_ptr_pair peer_streams,
                             process_information_ptr peer_ptr      ) {
    send2mm(std::move(peer_streams), std::move(peer_ptr));
}

}} // namespace cppa::detail

#endif // CPPA_MAILMAN_HPP
