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


#ifndef CPPA_POST_OFFICE_HPP
#define CPPA_POST_OFFICE_HPP

#include <memory>
#include <utility>

#include "cppa/atom.hpp"
#include "cppa/actor_proxy.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/util/acceptor.hpp"
#include "cppa/util/io_stream.hpp"

#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa { namespace detail {

enum class po_message_type {
    add_peer,
    rm_peer,
    publish,
    unpublish,
    shutdown
};

struct po_message {
    po_message* next;
    const po_message_type type;
    union {
        std::pair<util::io_stream_ptr_pair, process_information_ptr> new_peer;
        util::io_stream_ptr_pair peer_streams;
        std::pair<std::unique_ptr<util::acceptor>, actor_ptr> new_published_actor;
        actor_ptr published_actor;
    };
    po_message();
    po_message(util::io_stream_ptr_pair, process_information_ptr);
    po_message(util::io_stream_ptr_pair);
    po_message(std::unique_ptr<util::acceptor>, actor_ptr);
    po_message(actor_ptr);
    ~po_message();
    template<typename... Args>
    static inline std::unique_ptr<po_message> create(Args&&... args) {
        return std::unique_ptr<po_message>(new po_message(std::forward<Args>(args)...));
    }
};

typedef intrusive::single_reader_queue<po_message> po_message_queue;

void post_office_loop(int input_fd, po_message_queue&);

template<typename... Args>
inline void send2po(Args&&... args) {
    auto nm = singleton_manager::get_network_manager();
    nm->send_to_post_office(po_message::create(std::forward<Args>(args)...));
}

inline void post_office_add_peer(util::io_stream_ptr_pair peer_streams,
                                 process_information_ptr peer_ptr      ) {
    send2po(std::move(peer_streams), std::move(peer_ptr));
}

inline void post_office_close_peer_connection(util::io_stream_ptr_pair peer_streams) {
    send2po(std::move(peer_streams));
}

inline void post_office_publish(std::unique_ptr<util::acceptor> server,
                                actor_ptr published_actor              ) {
    send2po(std::move(server), std::move(published_actor));
}

inline void post_office_unpublish(actor_ptr whom) {
    send2po(std::move(whom));
}

} } // namespace cppa::detail

#endif // CPPA_POST_OFFICE_HPP
