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


#ifndef CPPA_IO_TCP_IO_STREAM_HPP
#define CPPA_IO_TCP_IO_STREAM_HPP

#include "cppa/config.hpp"
#include "cppa/io/stream.hpp"

namespace cppa {
namespace io {

/**
 * @brief An implementation of the {@link stream} interface for TCP sockets.
 */
class tcp_io_stream : public stream {

 public:

    ~tcp_io_stream();

    /**
     * @brief Establishes a TCP connection to given @p host at given @p port.
     * @throws network_error if connection fails or read error occurs
     */
    static stream_ptr connect_to(const char* host, std::uint16_t port);

    /**
     * @brief Creates an TCP stream from the native socket handle @p fd.
     */
    static stream_ptr from_sockfd(native_socket_type fd);

    native_socket_type read_handle() const;

    native_socket_type write_handle() const;

    void read(void* buf, size_t len);

    size_t read_some(void* buf, size_t len);

    void write(const void* buf, size_t len);

    size_t write_some(const void* buf, size_t len);

 private:

    tcp_io_stream(native_socket_type fd);

    native_socket_type m_fd;

};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_TCP_IO_STREAM_HPP
