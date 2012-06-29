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


#ifndef POST_OFFICE_MSG_HPP
#define POST_OFFICE_MSG_HPP

#include "cppa/attachable.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/process_information.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/intrusive/single_reader_queue.hpp"

namespace cppa { namespace detail {

class post_office_msg {

    friend class intrusive::singly_linked_list<post_office_msg>;
    friend class intrusive::single_reader_queue<post_office_msg>;

 public:

    enum msg_type {
        invalid_type,
        add_peer_type,
        add_server_socket_type,
        proxy_exited_type
    };

    struct add_peer {

        native_socket_type sockfd;
        process_information_ptr peer;
        actor_proxy_ptr first_peer_actor;
        std::unique_ptr<attachable> attachable_ptr;

        add_peer(native_socket_type peer_socket,
                 const process_information_ptr& peer_ptr,
                 const actor_proxy_ptr& peer_actor_ptr,
                 std::unique_ptr<attachable>&& peer_observer);

    };

    struct add_server_socket {

        native_socket_type server_sockfd;
        actor_ptr published_actor;

        add_server_socket(native_socket_type ssockfd, const actor_ptr& whom);

    };

    struct proxy_exited {
        actor_proxy_ptr proxy_ptr;
        inline proxy_exited(const actor_proxy_ptr& who) : proxy_ptr(who) { }
    };

    inline post_office_msg() : next(nullptr), m_type(invalid_type) { }

    post_office_msg(native_socket_type arg0,
                    const process_information_ptr& arg1,
                    const actor_proxy_ptr& arg2,
                    std::unique_ptr<attachable>&& arg3);

    post_office_msg(native_socket_type arg0, const actor_ptr& arg1);

    post_office_msg(const actor_proxy_ptr& proxy_ptr);

    inline bool is_add_peer_msg() const {
        return m_type == add_peer_type;
    }

    inline bool is_add_server_socket_msg() const {
        return m_type == add_server_socket_type;
    }

    inline bool is_proxy_exited_msg() const {
        return m_type == proxy_exited_type;
    }

    inline add_peer& as_add_peer_msg() {
        return m_add_peer_msg;
    }

    inline add_server_socket& as_add_server_socket_msg() {
        return m_add_server_socket;
    }

    inline proxy_exited& as_proxy_exited_msg() {
        return m_proxy_exited;
    }

    ~post_office_msg();

 private:

    post_office_msg* next;

    msg_type m_type;

    union {
        add_peer m_add_peer_msg;
        add_server_socket m_add_server_socket;
        proxy_exited m_proxy_exited;
    };

};

constexpr std::uint32_t rd_queue_event = 0x00;
constexpr std::uint32_t unpublish_actor_event = 0x01;
constexpr std::uint32_t close_socket_event = 0x02;
constexpr std::uint32_t shutdown_event = 0x03;

typedef std::uint32_t pipe_msg[2];
constexpr size_t pipe_msg_size = 2 * sizeof(std::uint32_t);

} } // namespace cppa::detail

#endif // POST_OFFICE_MSG_HPP
