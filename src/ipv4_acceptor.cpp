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


#include <ios>
#include <cstring>
#include <errno.h>
#include <iostream>
#include "cppa/exception.hpp"
#include "cppa/util/io_stream.hpp"
#include "cppa/detail/ipv4_acceptor.hpp"
#include "cppa/detail/ipv4_io_stream.hpp"

#ifdef CPPA_WINDOWS
#else
#   include <netdb.h>
#   include <unistd.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <fcntl.h>
#endif

namespace cppa { namespace detail {

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

int rd_flags(native_socket_type fd) {
    auto flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw network_error("unable to read socket flags");
    }
    return flags;
}

bool accept_impl(util::io_stream_ptr_pair& result, native_socket_type fd, bool nonblocking) {
    sockaddr addr;
    socklen_t addrlen;
    memset(&addr, 0, sizeof(addr));
    memset(&addrlen, 0, sizeof(addrlen));
    auto sfd = ::accept(fd, &addr, &addrlen);
    if (sfd < 0) {
        if (nonblocking && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // ok, try again
            return false;
        }
        char* cstr = strerror(errno);
        std::string errmsg = cstr;
        free(cstr);
        throw std::ios_base::failure(std::move(errmsg));
    }
    int flags = 1;
    setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(int));
    util::io_stream_ptr ptr(ipv4_io_stream::from_native_socket(sfd));
    result.first = ptr;
    result.second = ptr;
    return true;
}

} // namespace <anonymous>

ipv4_acceptor::ipv4_acceptor(native_socket_type fd, bool nonblocking)
: m_fd(fd), m_is_nonblocking(nonblocking) { }

std::unique_ptr<util::acceptor> ipv4_acceptor::create(std::uint16_t port) {
    native_socket_type sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == invalid_socket) {
        throw network_error("could not create server socket");
    }
    // sguard closes the socket in case of an exception
    socket_guard sguard(sockfd);
    memset((char*) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        throw bind_failure(errno);
    }
    if (listen(sockfd, 10) != 0) {
        throw network_error("listen() failed");
    }
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        throw network_error("unable to get socket flags");
    }
    bool nonblocking = (flags & O_NONBLOCK) != 0;
    // ok, no exceptions
    sguard.release();
    std::unique_ptr<ipv4_acceptor> result(new ipv4_acceptor(sockfd, nonblocking));
    return std::move(result);
}

ipv4_acceptor::~ipv4_acceptor() {
    closesocket(m_fd);
}

native_socket_type ipv4_acceptor::acceptor_file_handle() const {
    return m_fd;
}

util::io_stream_ptr_pair ipv4_acceptor::accept_connection() {
    if (m_is_nonblocking) {
        auto flags = rd_flags(m_fd);
        if (fcntl(m_fd, F_SETFL, flags & (~(O_NONBLOCK))) < 0) {
            throw network_error("unable to set socket to nonblock");
        }
        m_is_nonblocking = false;
    }
    util::io_stream_ptr_pair result;
    accept_impl(result, m_fd, m_is_nonblocking);
    return result;
}

option<util::io_stream_ptr_pair> ipv4_acceptor::try_accept_connection() {
    if (!m_is_nonblocking) {
        auto flags = rd_flags(m_fd);
        if (fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            throw network_error("unable to set socket to nonblock");
        }
        m_is_nonblocking = true;
    }
    util::io_stream_ptr_pair result;
    if (accept_impl(result, m_fd, m_is_nonblocking)) {
        return result;
    }
    return {};
}

} } // namespace cppa::detail
