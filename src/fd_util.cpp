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


#include <ios>
#include <sstream>

#include "cppa/exception.hpp"
#include "cppa/detail/fd_util.hpp"

#if defined(CPPA_LINUX) || defined(CPPA_MACOS)

#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

namespace cppa { namespace detail { namespace fd_util {

void throw_io_failure(const char* what, bool add_errno_failure) {
    if (add_errno_failure) {
        std::ostringstream oss;
        oss << what << ": " << strerror(errno)
            << " [errno: " << errno << "]";
        throw std::ios_base::failure(oss.str());
    }
    throw std::ios_base::failure(what);
}

int rd_flags(native_socket_type fd) {
    auto flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) throw_io_failure("unable to read socket flags");
    return flags;
}

bool nonblocking(native_socket_type fd) {
    return (rd_flags(fd) & O_NONBLOCK) != 0;
}

void nonblocking(native_socket_type fd, bool new_value) {
    auto rf = rd_flags(fd);
    auto wf = new_value ? (rf | O_NONBLOCK) : (rf & (~(O_NONBLOCK)));
    if (fcntl(fd, F_SETFL, wf) < 0) {
        throw_io_failure("unable to set file descriptor flags");
    }
}

bool tcp_nodelay(native_socket_type fd) {
    int flag;
    auto len = static_cast<socklen_t>(sizeof(flag));
    if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, &len) < 0) {
        throw_io_failure("unable to read TCP_NODELAY socket option");
    }
    return flag != 0;
}

void tcp_nodelay(native_socket_type fd, bool new_value) {
    int flag = new_value ? 1 : 0;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        throw_io_failure("unable to set TCP_NODELAY");
    }
}

void handle_io_result(ssize_t res, bool is_nonblock, const char* msg) {
    if (res < 0) {
        // don't throw for 'failed' non-blocking IO
        if (is_nonblock && (errno == EAGAIN || errno == EWOULDBLOCK)) { }
        else throw_io_failure(msg);
    }
}

void handle_write_result(ssize_t res, bool is_nonblock) {
    handle_io_result(res, is_nonblock, "cannot write to file descriptor");
}

void handle_read_result(ssize_t res, bool is_nonblock) {
    handle_io_result(res, is_nonblock, "cannot read from file descriptor");
    if (res == 0) throw_io_failure("cannot read from closed file descriptor");
}

} } } // namespace cppa::detail::fd_util

#else
// windows
#include <winsock2.h>

namespace cppa { namespace detail { namespace fd_util {

void throw_io_failure(const char* what, bool add_errno_failure) {
    if (add_errno_failure) {
        std::ostringstream oss;
        oss << what << ": " << strerror(errno)
            << " [errno: " << errno << "]";
        throw std::ios_base::failure(oss.str());
    }
    throw std::ios_base::failure(what);
}

// int rd_flags(native_socket_type fd) {

//     auto flags = ioctlsocket(fd, F_GETFL, 0);
//     if (flags == -1) throw_io_failure("unable to read socket flags");
//     return flags;
// }

// bool nonblocking(native_socket_type fd) {    
//     return (rd_flags(fd) & O_NONBLOCK) != 0;
// }

void nonblocking(native_socket_type fd, bool new_value) {
    u_long iMode = new_value ? 1 : 0;
// If iMode = 0, blocking is enabled; 
// If iMode != 0, non-blocking mode is enabled.
    if (ioctlsocket(fd, FIONBIO, &iMode) < 0) {
        throw_io_failure("unable to set FIONBIO");
    }
}

bool tcp_nodelay(native_socket_type fd) {
    int flag;
    auto len = static_cast<int>(sizeof(flag));
    if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>  (&flag), &len) < 0) {
        throw_io_failure("unable to read TCP_NODELAY socket option");
    }
    return flag != 0;
}

void tcp_nodelay(native_socket_type fd, bool new_value) {
    int flag = new_value ? 1 : 0;
    auto len = static_cast<int>(sizeof(flag));
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *> (&flag), len) < 0) {
        throw_io_failure("unable to set TCP_NODELAY");
    }
}

void handle_io_result(ssize_t res, bool is_nonblock, const char* msg) {
    if (res < 0) {
        // don't throw for 'failed' non-blocking IO
        if (is_nonblock && ( WSAGetLastError() == WSAEWOULDBLOCK)) { }
        else throw_io_failure(msg);
    }
}

void handle_write_result(ssize_t res, bool is_nonblock) {
    handle_io_result(res, is_nonblock, "cannot write to file descriptor");
}

void handle_read_result(ssize_t res, bool is_nonblock) {
    handle_io_result(res, is_nonblock, "cannot read from file descriptor");
    if (res == 0) throw_io_failure("cannot read from closed file descriptor");
}

} } } // namespace cppa::detail::fd_util
#endif
