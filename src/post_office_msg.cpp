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


#include "cppa/detail/post_office_msg.hpp"

namespace cppa { namespace detail {

post_office_msg::add_peer::add_peer(native_socket_type peer_socket,
                                    const process_information_ptr& peer_ptr,
                                    const actor_proxy_ptr& peer_actor_ptr,
                                    std::unique_ptr<attachable>&& peer_observer)
    : sockfd(peer_socket)
    , peer(peer_ptr)
    , first_peer_actor(peer_actor_ptr)
    , attachable_ptr(std::move(peer_observer))
{
}

post_office_msg::add_server_socket::add_server_socket(native_socket_type ssockfd,
                                                      const actor_ptr& whom)
    : server_sockfd(ssockfd)
    , published_actor(whom)
{
}

post_office_msg::post_office_msg(native_socket_type arg0,
                                 const process_information_ptr& arg1,
                                 const actor_proxy_ptr& arg2,
                                 std::unique_ptr<attachable>&& arg3)
    : next(nullptr)
    , m_type(add_peer_type)
{
    new (&m_add_peer_msg) add_peer(arg0, arg1, arg2, std::move(arg3));
}

post_office_msg::post_office_msg(native_socket_type arg0, const actor_ptr& arg1)
    : next(nullptr)
    , m_type(add_server_socket_type)
{
    new (&m_add_server_socket) add_server_socket(arg0, arg1);
}

post_office_msg::post_office_msg(const actor_proxy_ptr& proxy_ptr)
    : next(nullptr)
    , m_type(proxy_exited_type)
{
    new (&m_proxy_exited) proxy_exited(proxy_ptr);
}

post_office_msg::~post_office_msg()
{
    switch (m_type)
    {
        case add_peer_type:
        {
            m_add_peer_msg.~add_peer();
            break;
        }
        case add_server_socket_type:
        {
            m_add_server_socket.~add_server_socket();
            break;
        }
        case proxy_exited_type:
        {
            m_proxy_exited.~proxy_exited();
            break;
        }
        default: throw std::logic_error("invalid post_office_msg type");
    }
}

} } // namespace cppa::detail
