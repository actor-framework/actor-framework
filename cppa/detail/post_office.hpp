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

#include "cppa/atom.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/detail/native_socket.hpp"

namespace cppa { namespace detail {

struct po_message {
    atom_value flag;
    native_socket_type fd;
    actor_id aid;
};

void post_office_loop(int input_fd);

void post_office_add_peer(native_socket_type peer_socket,
                          const process_information_ptr& peer_ptr);

void post_office_publish(native_socket_type server_socket,
                         const actor_ptr& published_actor);

void post_office_unpublish(actor_id whom);

void post_office_close_socket(native_socket_type sfd);

} } // namespace cppa::detail

#endif // CPPA_POST_OFFICE_HPP
