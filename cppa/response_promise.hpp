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

#ifndef CPPA_RESPONSE_PROMISE_HPP
#define CPPA_RESPONSE_PROMISE_HPP

#include "cppa/actor.hpp"
#include "cppa/message.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/message_id.hpp"

namespace cppa {

/**
 * @brief A response promise can be used to deliver a uniquely identifiable
 *        response message from the server (i.e. receiver of the request)
 *        to the client (i.e. the sender of the request).
 */
class response_promise {

 public:

    response_promise() = default;
    response_promise(response_promise&&) = default;
    response_promise(const response_promise&) = default;
    response_promise& operator=(response_promise&&) = default;
    response_promise& operator=(const response_promise&) = default;

    response_promise(const actor_addr& from, const actor_addr& to,
                     const message_id& response_id);

    /**
     * @brief Queries whether this promise is still valid, i.e., no response
     *        was yet delivered to the client.
     */
    inline explicit operator bool() const {
        // handle is valid if it has a receiver
        return static_cast<bool>(m_to);
    }

    /**
     * @brief Sends @p response_message and invalidates this handle afterwards.
     */
    void deliver(message response_message);

 private:

    actor_addr m_from;
    actor_addr m_to;
    message_id m_id;

};

} // namespace cppa

#endif // CPPA_RESPONSE_PROMISE_HPP
