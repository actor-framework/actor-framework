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
void handle_syscall_result(T result, bool is_recv_result) {
    if (result < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            char* cstr = strerror(errno);
            std::string errmsg = cstr;
            errmsg += " [errno = ";
            errmsg += std::to_string(errno);
            errmsg += "]";
std::cerr << errmsg << std::endl;
            throw std::ios_base::failure(std::move(errmsg));
        }
    }
    else if (result == 0 && is_recv_result) {
        throw std::ios_base::failure("cannot read from closed socket");
    }
}

int rd_flags(native_socket_type fd) {
    auto flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw network_error("unable to read socket flags");
    }
    return flags;
}

void set_nonblocking(native_socket_type fd) {
    auto flags = rd_flags(fd);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw network_error("unable to set socket to nonblock");
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

void ipv4_io_stream::read(void* vbuf, size_t len) {
    auto buf = reinterpret_cast<char*>(vbuf);
    size_t rd = 0;
    while (rd < len) {
        auto recv_result = ::recv(m_fd, buf + rd, len - rd, 0);
        handle_syscall_result(recv_result, true);
        if (recv_result > 0) {
            rd += static_cast<size_t>(recv_result);
        }
        if (rd < len) {
            fd_set rdset;
            FD_ZERO(&rdset);
            FD_SET(m_fd, &rdset);
            if (select(m_fd + 1, &rdset, nullptr, nullptr, nullptr) < 0) {
                throw network_error("select() failed");
            }
        }
    }
}

size_t ipv4_io_stream::read_some(void* buf, size_t len) {
    auto recv_result = ::recv(m_fd, buf, len, 0);
    handle_syscall_result(recv_result, true);
    return (recv_result > 0) ? static_cast<size_t>(recv_result) : 0;
}

void ipv4_io_stream::write(const void* vbuf, size_t len) {
    auto buf = reinterpret_cast<const char*>(vbuf);
    size_t written = 0;
    while (written < len) {
        auto send_result = ::send(m_fd, buf + written, len - written, 0);
        handle_syscall_result(send_result, false);
        if (send_result > 0) {
            written += static_cast<size_t>(send_result);
        }
        if (written < len) {
            // block until socked is writable again
            fd_set wrset;
            FD_ZERO(&wrset);
            FD_SET(m_fd, &wrset);
            if (select(m_fd + 1, nullptr, &wrset, nullptr, nullptr) < 0) {
                throw network_error("select() failed");
            }
        }
    }
}

size_t ipv4_io_stream::write_some(const void* buf, size_t len) {
    auto send_result = ::send(m_fd, buf, len, 0);
    handle_syscall_result(send_result, false);
    return static_cast<size_t>(send_result);
}

util::io_stream_ptr ipv4_io_stream::from_native_socket(native_socket_type fd) {
    set_nonblocking(fd);
    return new ipv4_io_stream(fd);
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
    set_nonblocking(sockfd);
    return new ipv4_io_stream(sockfd);
}

} } // namespace cppa::detail
