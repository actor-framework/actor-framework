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


#include <sstream>
#include <cstring>

#include "cppa/config.hpp"
#include "cppa/exception.hpp"

#ifdef CPPA_WINDOWS
#   include <winsock2.h>
#   include <ws2tcpip.h> /* socklen_t, et al (MSVC20xx) */
#   include <windows.h>
#   include <io.h>
#else
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <errno.h>
#   include <netdb.h>
#   include <fcntl.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#endif

#include "cppa/util/scope_guard.hpp"

#include "cppa/detail/fd_util.hpp"

/************** implementation of platform-independent functions **************/

namespace cppa {
namespace detail {
namespace fd_util {

void throw_io_failure [[noreturn]] (const char* what, bool add_errno) {
    if (add_errno) {
        std::ostringstream oss;
        oss << what << ": " << last_socket_error_as_string()
            << " [errno: " << last_socket_error() << "]";
        throw network_error(oss.str());
    }
    throw network_error(what);
}

void tcp_nodelay(native_socket_type fd, bool new_value) {
    int flag = new_value ? 1 : 0;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<setsockopt_ptr>(&flag), sizeof(flag)) < 0) {
        throw_io_failure("unable to set TCP_NODELAY");
    }
}

void handle_io_result(ssize_t res, bool is_nonblock, const char* msg) {
    if (res < 0) {
        auto err = last_socket_error();
        if (is_nonblock && would_block_or_temporarily_unavailable(err)) {
            // don't throw for 'failed' non-blocking IO,
            // just try again later
        }
        else throw_io_failure(msg);
    }
}

void handle_write_result(ssize_t res, bool is_nonblock) {
    handle_io_result(res, is_nonblock, "cannot write to file descriptor");
}

void handle_read_result(ssize_t res, bool is_nonblock) {
    handle_io_result(res, is_nonblock, "cannot read from socket");
    if (res == 0) throw stream_at_eof("cannot read from closed socket");
}

} // namespace fd_util
} // namespace detail
} // namespace cppa

#ifndef CPPA_WINDOWS // Linux or Mac OS

namespace cppa {
namespace detail {
namespace fd_util {

std::string last_socket_error_as_string() {
    return strerror(errno);
}

void nonblocking(native_socket_type fd, bool new_value) {
    // read flags for fd
    auto rf = fcntl(fd, F_GETFL, 0);
    if (rf == -1) throw_io_failure("unable to read socket flags");
    // calculate and set new flags
    auto wf = new_value ? (rf | O_NONBLOCK) : (rf & (~(O_NONBLOCK)));
    if (fcntl(fd, F_SETFL, wf) < 0) {
        throw_io_failure("unable to set file descriptor flags");
    }
}

std::pair<native_socket_type, native_socket_type> create_pipe() {
    native_socket_type pipefds[2];
    if (pipe(pipefds) != 0) { CPPA_CRITICAL("cannot create pipe"); }
    return {pipefds[0], pipefds[1]};
}

} // namespace fd_util
} // namespace detail
} // namespace cppa

#else // CPPA_WINDOWS

namespace cppa {
namespace detail {
namespace fd_util {

std::string last_socket_error_as_string() {
    LPTSTR errorText = NULL;
    auto hresult = last_socket_error();
    FormatMessage(// use system message tables to retrieve error text
                  FORMAT_MESSAGE_FROM_SYSTEM
                  // allocate buffer on local heap for error text
                  | FORMAT_MESSAGE_ALLOCATE_BUFFER
                  // Important! will fail otherwise, since we're not
                  // (and CANNOT) pass insertion parameters
                  | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, // unused with FORMAT_MESSAGE_FROM_SYSTEM
                  hresult,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) &errorText,  // output
                  0, // minimum size for output buffer
                  nullptr);   // arguments - see note
    std::string result;
    if (errorText != nullptr) {
        result = errorText;
       // release memory allocated by FormatMessage()
       LocalFree(errorText);
    }
    return result;
}

void nonblocking(native_socket_type fd, bool new_value) {
    u_long mode = new_value ? 1 : 0;
    if (ioctlsocket(fd, FIONBIO, &mode) < 0) {
        throw_io_failure("unable to set FIONBIO");
    }
}

// wraps a C call and throws an IO failure if f returns an integer != 0
template<typename F, typename... Ts>
inline void ccall(const char* errmsg, F f, Ts&&... args) {
    if (f(std::forward<Ts>(args)...) != 0) {
        throw_io_failure(errmsg);
    }
}

/******************************************************************************\
 * Based on work of others;                                                   *
 * original header:                                                           *
 *                                                                            *
 * Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>                  *
 * Redistribution and use in source and binary forms, with or without         *
 * modification, are permitted provided that the following conditions are met:*
 *                                                                            *
 * Redistributions of source code must retain the above copyright notice,     *
 * this list of conditions and the following disclaimer.                      *
 *                                                                            *
 * Redistributions in binary form must reproduce the above copyright notice,  *
 * this list of conditions and the following disclaimer in the documentation  *
 * and/or other materials provided with the distribution.                     *
 *                                                                            *
 * The name of the author must not be used to endorse or promote products     *
 * derived from this software without specific prior written permission.      *
 *                                                                            *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS        *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED  *
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR *
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR          *
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,      *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,        *
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;*
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,   *
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR    *
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF     *
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                 *
\******************************************************************************/
std::pair<native_socket_type, native_socket_type> create_pipe() {
    socklen_t addrlen = sizeof(sockaddr_in);
    native_socket_type socks[2] = {invalid_socket, invalid_socket};
    auto listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == invalid_socket) throw_io_failure("socket() failed");
    union {
        sockaddr_in inaddr;
        sockaddr addr;
    } a;
    memset(&a, 0, sizeof(a));
    a.inaddr.sin_family = AF_INET;
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_port = 0;
    bool success = false;
    // makes sure all sockets are closed in case of an error
    auto guard = util::make_scope_guard([&] {
        if (success) return; // everyhting's fine
        auto e = WSAGetLastError();
        closesocket(listener);
        closesocket(socks[0]);
        closesocket(socks[1]);
        WSASetLastError(e);
    });
    // bind listener to a local port
    int reuse = 1;
    ccall("setsockopt() failed", ::setsockopt, listener,
          SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuse),
          static_cast<socklen_t>(sizeof(reuse)));
    ccall("bind() failed", ::bind, listener, &a.addr, sizeof(a.inaddr));
    // read the port in use: win32 getsockname may only set the port number
    // (http://msdn.microsoft.com/library/ms738543.aspx):
    memset(&a, 0, sizeof(a));
    ccall("getsockname() failed", ::getsockname, listener, &a.addr, &addrlen);
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_family = AF_INET;
    // set listener to listen mode
    ccall("listen() failed", ::listen, listener, 1);
    // create read-only end of the pipe
    DWORD flags = 0;
    auto read_fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags);
    if (read_fd == invalid_socket) {
        throw_io_failure("cannot create read handle: WSASocket() failed");
    }
    ccall("connect() failed", ::connect, read_fd, &a.addr, sizeof(a.inaddr));
    // get write-only end of the pipe
    auto write_fd = accept(listener, NULL, NULL);
    if (write_fd == invalid_socket) {
        throw_io_failure("cannot create write handle: accept() failed");
    }
    closesocket(listener);
    success = true;
    return {read_fd, write_fd};
}

} // namespace fd_util
} // namespace util
} // namespace cppa

#endif
