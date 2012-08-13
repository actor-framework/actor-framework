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

#include "cppa/exception.hpp"
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

template<typename T>
void handle_syscall_result(T result, size_t num_bytes, bool nonblocking) {
    if (result < 0) {
        if (!nonblocking || errno != EAGAIN) {
            char* cstr = strerror(errno);
            std::string errmsg = cstr;
            free(cstr);
            throw std::ios_base::failure(std::move(errmsg));
        }
    }
    else if (result == 0) {
        throw std::ios_base::failure("cannot read/write from closed socket");
    }
    else if (!nonblocking && static_cast<size_t>(result) != num_bytes) {
        throw std::ios_base::failure("IO error on IPv4 socket");
    }
}

} // namespace <anonymous>

ipv4_io_stream::ipv4_io_stream(native_socket_type fd) : m_fd(fd) { }

native_socket_type ipv4_io_stream::read_file_handle() const {
    return m_fd;
}

native_socket_type ipv4_io_stream::write_file_handle() const {
    return m_fd;
}

void ipv4_io_stream::read(void* buf, size_t len) {
    handle_syscall_result(::recv(m_fd, buf, len, 0), len, false);
}

size_t ipv4_io_stream::read_some(void* buf, size_t len) {
    auto recv_result = ::recv(m_fd, buf, len, MSG_DONTWAIT);
    handle_syscall_result(recv_result, len, true);
    return static_cast<size_t>(recv_result);
}

void ipv4_io_stream::write(const void* buf, size_t len) {
    handle_syscall_result(::send(m_fd, buf, len, 0), len, false);
}

size_t ipv4_io_stream::write_some(const void* buf, size_t len) {
    auto send_result = ::send(m_fd, buf, len, MSG_DONTWAIT);
    handle_syscall_result(send_result, len, true);
    return static_cast<size_t>(send_result);
}

util::io_stream_ptr ipv4_io_stream::connect_to(const char* host,
                                               std::uint16_t port) {
    native_socket_type sockfd;
    struct sockaddr_in serv_addr;
    struct hostent* server;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == invalid_socket) {
        throw network_error("socket creation failed");
    }
    server = gethostbyname(host);
    if (!server) {
        std::string errstr = "no such host: ";
        errstr += host;
        throw network_error(std::move(errstr));
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memmove(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(sockfd, (const sockaddr*) &serv_addr, sizeof(serv_addr)) != 0) {
        throw network_error("could not connect to host");
    }
    int flags = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(int));
    return new ipv4_io_stream(sockfd);
}

} } // namespace cppa::detail
