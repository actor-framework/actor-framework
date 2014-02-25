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
 * Copyright (C) 2011-2013                                                    *
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


#ifndef CPPA_IPV4_IO_STREAM_HPP
#define CPPA_IPV4_IO_STREAM_HPP

#include "cppa/config.hpp"
#include "cppa/io/stream.hpp"

namespace cppa { namespace io {

class ipv4_io_stream : public stream {

 public:

    static stream_ptr connect_to(const char* host, std::uint16_t port);

    static stream_ptr from_native_socket(native_socket_type fd);

    native_socket_type read_handle() const;

    native_socket_type write_handle() const;

    void read(void* buf, size_t len);

    size_t read_some(void* buf, size_t len);

    void write(const void* buf, size_t len);

    size_t write_some(const void* buf, size_t len);

 private:

    ipv4_io_stream(native_socket_type fd);

    native_socket_type m_fd;

};

} } // namespace cppa::detail

#endif // CPPA_IPV4_IO_STREAM_HPP
