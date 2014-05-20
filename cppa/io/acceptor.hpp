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
 * Copyright (C) 2011-2014                                                    *
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


#ifndef CPPA_IO_ACCEPTOR_HPP
#define CPPA_IO_ACCEPTOR_HPP

#include <memory>

#include "cppa/config.hpp"
#include "cppa/optional.hpp"

#include "cppa/io/input_stream.hpp"
#include "cppa/io/output_stream.hpp"

namespace cppa {
namespace io {

/**
 * @brief A pair of input and output stream pointers.
 */
typedef std::pair<input_stream_ptr, output_stream_ptr> stream_ptr_pair;

/**
 * @brief Accepts connections from client processes.
 */
class acceptor {

 public:

    virtual ~acceptor();

    /**
     * @brief Returns the internal file descriptor. This descriptor is needed
     *        for socket multiplexing using select()/poll()/epoll().
     */
    virtual native_socket_type file_handle() const = 0;

    /**
     * @brief Accepts a new connection and returns an input/output stream pair.
     * @note This member function blocks until a new connection was established.
     */
    virtual stream_ptr_pair accept_connection() = 0;

    /**
     * @brief Tries to accept a new connection but immediately returns if
     *        there is no pending connection.
     */
    virtual optional<stream_ptr_pair> try_accept_connection() = 0;

};

typedef std::unique_ptr<acceptor> acceptor_uptr;

} // namespace io
} // namespace cppa

#endif // CPPA_IO_ACCEPTOR_HPP
