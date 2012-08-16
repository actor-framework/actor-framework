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


#ifndef MIDDLEMAN_HPP
#define MIDDLEMAN_HPP

#include <memory>

#include "cppa/actor.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/acceptor.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/addressed_message.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa { namespace detail {

enum class middleman_message_type {
    add_peer,
    publish,
    unpublish,
    outgoing_message,
    shutdown
};

struct middleman_message {
    middleman_message* next;
    const middleman_message_type type;
    union {
        std::pair<util::io_stream_ptr_pair, process_information_ptr> new_peer;
        std::pair<std::unique_ptr<util::acceptor>, actor_ptr> new_published_actor;
        actor_ptr published_actor;
        std::pair<process_information_ptr, addressed_message> out_msg;
    };
    middleman_message();
    middleman_message(util::io_stream_ptr_pair, process_information_ptr);
    middleman_message(std::unique_ptr<util::acceptor>, actor_ptr);
    middleman_message(process_information_ptr, addressed_message);
    middleman_message(actor_ptr);
    ~middleman_message();
    template<typename... Args>
    static inline std::unique_ptr<middleman_message> create(Args&&... args) {
        return std::unique_ptr<middleman_message>(new middleman_message(std::forward<Args>(args)...));
    }
};

typedef intrusive::single_reader_queue<middleman_message> middleman_queue;

void middleman_loop(int pipe_rd, middleman_queue& queue);

template<typename... Args>
inline void send2mm(Args&&... args) {
    auto nm = singleton_manager::get_network_manager();
    nm->send_to_middleman(middleman_message::create(std::forward<Args>(args)...));
}

inline void middleman_add_peer(util::io_stream_ptr_pair peer_streams,
                                 process_information_ptr peer_ptr      ) {
    send2mm(std::move(peer_streams), std::move(peer_ptr));
}

inline void middleman_publish(std::unique_ptr<util::acceptor> server,
                                actor_ptr published_actor              ) {
    send2mm(std::move(server), std::move(published_actor));
}

inline void middleman_unpublish(actor_ptr whom) {
    send2mm(std::move(whom));
}

inline void middleman_enqueue(process_information_ptr peer,
                              addressed_message outgoing_message) {
    send2mm(std::move(peer), std::move(outgoing_message));
}

inline void middleman_enqueue(process_information_ptr peer,
                              actor_ptr sender,
                              channel_ptr receiver,
                              any_tuple&& msg,
                              message_id_t id = message_id_t()) {
    addressed_message amsg(std::move(sender), std::move(receiver),
                           std::move(msg), id);
    send2mm(std::move(peer), std::move(amsg));
}

} } // namespace cppa::detail

#endif // MIDDLEMAN_HPP
