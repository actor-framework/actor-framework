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


#ifndef CPPA_RESPONSE_HANDLE_HPP
#define CPPA_RESPONSE_HANDLE_HPP

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/message_id.hpp"

namespace cppa {

/**
 * @brief Denotes an outstanding response.
 */
class response_handle {

 public:

    response_handle() = default;
    response_handle(response_handle&&) = default;
    response_handle(const response_handle&) = default;
    response_handle& operator=(response_handle&&) = default;
    response_handle& operator=(const response_handle&) = default;

    response_handle(const actor_addr& from,
                    const actor_addr& to,
                    const message_id& response_id);

    /**
     * @brief Queries whether response message is still outstanding.
     */
    bool valid() const;

    /**
     * @brief Queries whether this is a response
     *        handle for a synchronous request.
     */
    bool synchronous() const;

    /**
     * @brief Sends @p response_message and invalidates this handle afterwards.
     */
    void apply(any_tuple response_message) const;

    /**
     * @brief Returns the message id for the response message.
     */
    inline const message_id& response_id() const { return m_id; }

    /**
     * @brief Returns the actor that is going send the response message.
     */
    inline const actor_addr& sender() const { return m_from; }

    /**
     * @brief Returns the actor that is waiting for the response message.
     */
    inline const actor_addr& receiver() const { return m_to; }

 private:

    actor_addr m_from;
    actor_addr m_to;
    message_id m_id;

};

} // namespace cppa


#endif // CPPA_RESPONSE_HANDLE_HPP
