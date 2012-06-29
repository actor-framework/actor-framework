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


#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <cstring>    // memset
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <netinet/tcp.h>

#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/detail/mailman.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/native_socket.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"
#include "cppa/detail/singleton_manager.hpp"

using std::cout;
using std::endl;

namespace cppa {

namespace {

void read_from_socket(detail::native_socket_type sfd, void* buf, size_t buf_size) {
    char* cbuf = reinterpret_cast<char*>(buf);
    size_t read_bytes = 0;
    size_t left = buf_size;
    int rres = 0;
    size_t urres = 0;
    do {
        rres = ::recv(sfd, cbuf + read_bytes, left, 0);
        if (rres <= 0) {
            throw std::ios_base::failure("cannot read from closed socket");
        }
        urres = static_cast<size_t>(rres);
        read_bytes += urres;
        left -= urres;
    }
    while (urres < left);
}

} // namespace <anonmyous>

struct socket_guard {

    bool m_released;
    detail::native_socket_type m_socket;

 public:

    socket_guard(detail::native_socket_type sfd) : m_released(false), m_socket(sfd) {
    }

    ~socket_guard() {
        if (!m_released) detail::closesocket(m_socket);
    }

    void release() {
        m_released = true;
    }

};

void publish(actor_ptr whom, std::uint16_t port) {
    if (!whom) return;
    detail::singleton_manager::get_actor_registry()->put(whom->id(), whom);
    detail::native_socket_type sockfd;
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == detail::invalid_socket) {
        throw network_error("could not create server socket");
    }
    // sguard closes the socket if an exception occurs
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
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw network_error("unable to set socket to nonblock");
    }
    flags = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(int));
    // ok, no exceptions
    sguard.release();
    detail::post_office_publish(sockfd, whom);
}

actor_ptr remote_actor(const char* host, std::uint16_t port) {
    detail::native_socket_type sockfd;
    struct sockaddr_in serv_addr;
    struct hostent* server;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == detail::invalid_socket) {
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
    auto pinf = process_information::get();
    std::uint32_t process_id = pinf->process_id();
    int flags = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(int));
    ::send(sockfd, &process_id, sizeof(std::uint32_t), 0);
    ::send(sockfd, pinf->node_id().data(), pinf->node_id().size(), 0);
    std::uint32_t remote_actor_id;
    std::uint32_t peer_pid;
    process_information::node_id_type peer_node_id;
    read_from_socket(sockfd, &remote_actor_id, sizeof(remote_actor_id));
    read_from_socket(sockfd, &peer_pid, sizeof(std::uint32_t));
    read_from_socket(sockfd, peer_node_id.data(), peer_node_id.size());

    flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        throw network_error("unable to get socket flags");
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw network_error("unable to set socket to nonblock");
    }

    auto peer_pinf = new process_information(peer_pid, peer_node_id);
    process_information_ptr pinfptr(peer_pinf);
    //auto key = std::make_tuple(remote_actor_id, pinfptr->process_id(), pinfptr->node_id());
    detail::singleton_manager::get_network_manager()
    ->send_to_mailman(make_any_tuple(sockfd, pinfptr));
    detail::post_office_add_peer(sockfd, pinfptr);
    return detail::get_actor_proxy_cache().get(remote_actor_id,
                                               pinfptr->process_id(),
                                               pinfptr->node_id());
    //auto ptr = get_scheduler()->register_hidden_context();
}

} // namespace cppa
