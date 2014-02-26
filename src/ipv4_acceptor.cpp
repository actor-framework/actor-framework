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
#include "cppa/config.hpp"
#ifdef CPPA_WINDOWS
#   include <ws2tcpip.h>
#   include <winsock2.h>
#endif

#include <ios>
#include <cstring>
#include <errno.h>
#include <iostream>

#include "cppa/logging.hpp"
#include "cppa/exception.hpp"

#include "cppa/io/stream.hpp"
#include "cppa/io/ipv4_acceptor.hpp"
#include "cppa/io/ipv4_io_stream.hpp"

#include "cppa/detail/fd_util.hpp"

#ifdef CPPA_WINDOWS
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <netdb.h>
#   include <unistd.h>
#   include <arpa/inet.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#endif

namespace cppa { namespace io {

using namespace ::cppa::detail::fd_util;

namespace {

struct socket_guard {

    bool m_released;
    native_socket_type m_socket;

 public:

    socket_guard(native_socket_type sfd) : m_released(false), m_socket(sfd) { }

    ~socket_guard() {
        if (!m_released) closesocket(m_socket);
    }

    void release() {
        m_released = true;
    }

};

bool accept_impl(stream_ptr_pair& result,
                 native_socket_type fd,
                 bool nonblocking) {
    sockaddr addr;
#ifdef CPPA_WINDOWS
    int addrlen;
#else
    socklen_t addrlen;
#endif
    memset(&addr, 0, sizeof(addr));
#ifdef CPPA_WINDOWS
    addrlen=sizeof(addr);   // check this

    auto sfd = ::WSAAccept(fd, &addr, &addrlen, 0, 0); // size too small
    if (sfd == INVALID_SOCKET) {
        if (nonblocking && (WSAGetLastError() == WSATRY_AGAIN || WSAGetLastError() == WSAEWOULDBLOCK)) {
            // ok, try again
            return false;
        }
        throw_io_failure("accept failed");
    }
#else
    memset(&addrlen, 0, sizeof(addrlen));
    auto sfd = ::accept(fd, &addr, &addrlen);
    if (sfd < 0) {
        if (nonblocking && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // ok, try again
            return false;
        }
        throw_io_failure("accept failed");
    }
#endif
    stream_ptr ptr(ipv4_io_stream::from_native_socket(sfd));
    result.first = ptr;
    result.second = ptr;
    return true;
}

} // namespace <anonymous>

ipv4_acceptor::ipv4_acceptor(native_socket_type fd, bool nonblocking)
: m_fd(fd), m_is_nonblocking(nonblocking) { }

std::unique_ptr<acceptor> ipv4_acceptor::create(std::uint16_t port,
                                                const char* addr) {
    CPPA_LOGM_TRACE("ipv4_acceptor", CPPA_ARG(port) << ", addr = "
                                     << (addr ? addr : "nullptr"));
    native_socket_type sockfd;

#ifdef CPPA_WINDOWS
    sockfd = INVALID_SOCKET;
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
        throw network_error("could not create server socket");
    }
#else
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == invalid_socket) {
        throw network_error("could not create server socket");
    }
#endif
    // sguard closes the socket in case of exception
    socket_guard sguard(sockfd);
#ifdef CPPA_WINDOWS
    int on = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on,sizeof(on)) < 0) {
        throw_io_failure("unable to set SO_REUSEADDR");
    }
#else
    int on = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        throw_io_failure("unable to set SO_REUSEADDR");
    }
#endif


    struct sockaddr_in serv_addr;
    memset((char*) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    if (! addr) {
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    }
#ifdef CPPA_WINDOWS
    else if (::inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
        throw network_error("invalid IPv4 address");
    }
#else
    else if (inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
        throw network_error("invalid IPv4 address");
    }
#endif

    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        throw bind_failure(errno);
    }
    if (listen(sockfd, 10) != 0) {
        throw network_error("listen() failed");
    }
    // default mode is nonblocking
    nonblocking(sockfd, true);
    // ok, no exceptions
    sguard.release();
    CPPA_LOGM_DEBUG("ipv4_acceptor", "sockfd = " << sockfd);
    return std::unique_ptr<ipv4_acceptor>(new ipv4_acceptor(sockfd, true));
}

ipv4_acceptor::~ipv4_acceptor() {
    closesocket(m_fd);
}

native_socket_type ipv4_acceptor::file_handle() const {
    return m_fd;
}

stream_ptr_pair ipv4_acceptor::accept_connection() {
    if (m_is_nonblocking) {
        nonblocking(m_fd, false);
        m_is_nonblocking = false;
    }
    stream_ptr_pair result;
    accept_impl(result, m_fd, m_is_nonblocking);
    return result;
}

optional<stream_ptr_pair> ipv4_acceptor::try_accept_connection() {
    if (!m_is_nonblocking) {
        nonblocking(m_fd, true);
        m_is_nonblocking = true;
    }
    stream_ptr_pair result;
    if (accept_impl(result, m_fd, m_is_nonblocking)) {
        return result;
    }
    return none;
}

} } // namespace cppa::detail
