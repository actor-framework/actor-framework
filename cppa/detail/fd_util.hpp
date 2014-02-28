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


#ifndef FD_UTIL_HPP
#define FD_UTIL_HPP

#include <unistd.h>
#include "cppa/config.hpp"

namespace cppa { namespace detail { namespace fd_util {

#if defined(CPPA_MACOS) || defined(CPPA_LINUX)

// throws ios_base::failure and adds errno failure if @p add_errno_failure
void throw_io_failure(const char* what, bool add_errno_failure = true);

// returns true if fd is nonblocking
// throws @p ios_base::failure on error
bool nonblocking(native_socket_type fd);

// sets fd to nonblocking if <tt>set_nonblocking == true</tt>
// or to blocking if <tt>set_nonblocking == false</tt>
// throws @p ios_base::failure on error
void nonblocking(native_socket_type fd, bool new_value);

// returns true if fd is nodelay socket
// throws @p ios_base::failure on error
bool tcp_nodelay(native_socket_type fd);

// returns true if fd is nodelay socket
// throws @p ios_base::failure on error
void tcp_nodelay(native_socket_type fd, bool new_value);

// reads @p result and @p errno and throws @p ios_base::failure on error
void handle_write_result(ssize_t result, bool is_nonblocking_io);

// reads @p result and @p errno and throws @p ios_base::failure on error
void handle_read_result(ssize_t result, bool is_nonblocking_io);

#else

// throws ios_base::failure and adds errno failure if @p add_errno_failure
void throw_io_failure(const char* what, bool add_errno_failure = true);

// returns true if fd is nonblocking
// throws @p ios_base::failure on error
bool nonblocking(native_socket_type fd);

// sets fd to nonblocking if <tt>set_nonblocking == true</tt>
// or to blocking if <tt>set_nonblocking == false</tt>
// throws @p ios_base::failure on error
void nonblocking(native_socket_type fd, bool new_value);

// returns true if fd is nodelay socket
// throws @p ios_base::failure on error
bool tcp_nodelay(native_socket_type fd);

// returns true if fd is nodelay socket
// throws @p ios_base::failure on error
void tcp_nodelay(native_socket_type fd, bool new_value);

// reads @p result and @p errno and throws @p ios_base::failure on error
void handle_write_result(ssize_t result, bool is_nonblocking_io);

// reads @p result and @p errno and throws @p ios_base::failure on error
void handle_read_result(ssize_t result, bool is_nonblocking_io);

#endif

} } } // namespace cppa::detail::fd_util

#endif // FD_UTIL_HPP
