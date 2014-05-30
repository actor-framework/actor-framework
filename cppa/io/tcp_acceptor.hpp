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


#ifndef CPPA_IO_TCP_ACCEPTOR_HPP
#define CPPA_IO_TCP_ACCEPTOR_HPP

#include <memory>
#include <cstdint>

#include "cppa/config.hpp"
#include "cppa/io/acceptor.hpp"

namespace cppa {
namespace io {

/**
 * @brief An implementation of the {@link acceptor} interface for TCP sockets.
 */
class tcp_acceptor : public acceptor {

 public:

    /**
     * @brief Creates an TCP acceptor and binds it to given @p port. Incoming
     *        connections are only accepted from the address @p addr.
     *        Per default, i.e., <tt>addr == nullptr</tt>, all incoming
     *        connections are accepted.
     * @throws network_error if a socket operation fails
     * @throws bind_failure if given port is already in use
     */
    static std::unique_ptr<acceptor> create(std::uint16_t port,
                                            const char* addr = nullptr);

    /**
     * @brief Creates an TCP acceptor from the native socket handle @p fd.
     */
    static std::unique_ptr<acceptor> from_sockfd(native_socket_type fd);

    ~tcp_acceptor();

    native_socket_type file_handle() const override;

    stream_ptr_pair accept_connection() override;

    optional<stream_ptr_pair> try_accept_connection() override;

 private:

    tcp_acceptor(native_socket_type fd, bool nonblocking);

    native_socket_type m_fd;
    bool m_is_nonblocking;

};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_TCP_ACCEPTOR_HPP
