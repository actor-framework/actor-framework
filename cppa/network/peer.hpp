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


#ifndef PEER_HPP
#define PEER_HPP

#include "cppa/network/addressed_message.hpp"
#include "cppa/network/continuable_reader.hpp"

enum continue_writing_result {
    write_failure,
    write_closed,
    write_continue_later,
    write_done
};


namespace cppa { namespace network {

class middleman;

/**
 * @brief Represents a bidirectional connection to a peer.
 */
class peer : public continuable_reader {

    typedef continuable_reader super;

 public:

    /**
     * @brief Returns the file descriptor for outgoing data.
     */
    native_socket_type write_handle() const {
        return m_write_handle;
    }

    /**
     * @brief Writes to {@link write_handle()}.
     */
    virtual continue_writing_result continue_writing() = 0;

    /**
     * @brief Enqueues @p msg to the list of outgoing messages.
     * @returns @p true on success, @p false otherwise.
     * @note Implementation should call {@link begin_writing()} and perform IO
     *       only in its implementation of {@link continue_writing()}.
     * @note Returning @p false from this function is interpreted as error
     *       and causes the middleman to remove this peer.
     */
    virtual bool enqueue(const addressed_message& msg) = 0;

 protected:

    /**
     * @brief Tells the middleman to add write_handle() to the list of
     *        observed sockets and to call continue_writing() if
     *        write_handle() is ready to write.
     * @note Does nothing if write_handle() is already registered for the
     *       event loop.
     */
    void begin_writing();

    void register_peer(const process_information& pinfo);

    peer(middleman* parent, native_socket_type rd, native_socket_type wr);

 private:

    native_socket_type m_write_handle;

};

typedef intrusive_ptr<peer> peer_ptr;

} } // namespace cppa::network

#endif // PEER_HPP
