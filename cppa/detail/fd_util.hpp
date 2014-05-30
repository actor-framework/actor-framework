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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_DETAIL_FD_UTIL_HPP
#define CPPA_DETAIL_FD_UTIL_HPP

#include <string>
#include <utility>  // std::pair
#include <unistd.h> // ssize_t

#include "cppa/config.hpp"

namespace cppa {
namespace detail {
namespace fd_util {

std::string last_socket_error_as_string();

// throws ios_base::failure and adds errno failure if @p add_errno_failure
void throw_io_failure [[noreturn]] (const char* what, bool add_errno = true);

// sets fd to nonblocking if <tt>set_nonblocking == true</tt>
// or to blocking if <tt>set_nonblocking == false</tt>
// throws @p ios_base::failure on error
void nonblocking(native_socket_type fd, bool new_value);

// returns true if fd is nodelay socket
// throws @p ios_base::failure on error
void tcp_nodelay(native_socket_type fd, bool new_value);

// reads @p result and @p errno and throws @p ios_base::failure on error
void handle_write_result(ssize_t result, bool is_nonblocking_io);

// reads @p result and @p errno and throws @p ios_base::failure on error
void handle_read_result(ssize_t result, bool is_nonblocking_io);

std::pair<native_socket_type, native_socket_type> create_pipe();

} // namespace fd_util
} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_FD_UTIL_HPP
