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


#ifndef CPPA_IPV4_IO_STREAM_HPP
#define CPPA_IPV4_IO_STREAM_HPP

#include "cppa/config.hpp"
#include "cppa/util/io_stream.hpp"

namespace cppa { namespace detail {

class ipv4_io_stream : public util::io_stream {

 public:

    static util::io_stream_ptr connect_to(const char* host, std::uint16_t port);

    ipv4_io_stream(native_socket_type fd);

    native_socket_type read_file_handle() const;

    native_socket_type write_file_handle() const;

    void read(void* buf, size_t len);

    size_t read_some(void* buf, size_t len);

    void write(const void* buf, size_t len);

    size_t write_some(const void* buf, size_t len);

 private:

    native_socket_type m_fd;

};

} } // namespace cppa::detail

#endif // CPPA_IPV4_IO_STREAM_HPP
