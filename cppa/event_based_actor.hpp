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


#ifndef CPPA_UNTYPED_ACTOR_HPP
#define CPPA_UNTYPED_ACTOR_HPP

#include "cppa/on.hpp"
#include "cppa/extend.hpp"
#include "cppa/logging.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/mailbox_based.hpp"
#include "cppa/response_handle.hpp"
#include "cppa/behavior_stack_based.hpp"

#include "cppa/detail/response_handle_util.hpp"

namespace cppa {

class event_based_actor;

/**
 * @brief A cooperatively scheduled, event-based actor implementation.
 *
 * This is the recommended base class for user-defined actors and is used
 * implicitly when spawning functor-based actors without the
 * {@link blocking_api} flag.
 *
 * @extends local_actor
 */
class event_based_actor : public extend<local_actor>::
                                 with<mailbox_based, behavior_stack_based> {

 protected:

    /**
     * @brief Returns the initial actor behavior.
     */
    virtual behavior make_behavior() = 0;

    /**
     * @brief Forwards the last received message to @p whom.
     */
    void forward_to(const actor& whom);

 public:

    typedef nonblocking_response_handle_tag response_handle_tag;

    typedef response_handle<event_based_actor> untyped_response_handle;

    /**
     * @brief Sends @p what as a synchronous message to @p whom.
     * @param whom Receiver of the message.
     * @param what Message content as tuple.
     * @returns A handle identifying a future to the response of @p whom.
     * @warning The returned handle is actor specific and the response to the
     *          sent message cannot be received by another actor.
     * @throws std::invalid_argument if <tt>whom == nullptr</tt>
     */
    inline untyped_response_handle sync_send_tuple(const actor& whom,
                                                any_tuple what) {
        return {sync_send_tuple_impl(message_priority::normal,
                                     whom, std::move(what)),
                this};
    }

    /**
     * @brief Sends <tt>{what...}</tt> as a synchronous message to @p whom.
     * @param whom Receiver of the message.
     * @param what Message elements.
     * @returns A handle identifying a future to the response of @p whom.
     * @warning The returned handle is actor specific and the response to the
     *          sent message cannot be received by another actor.
     * @pre <tt>sizeof...(Ts) > 0</tt>
     * @throws std::invalid_argument if <tt>whom == nullptr</tt>
     */
    template<typename... Ts>
    inline untyped_response_handle sync_send(const actor& whom, Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return sync_send_tuple(whom, make_any_tuple(std::forward<Ts>(what)...));
    }

    /**
     * @brief Sends a synchronous message with timeout @p rtime to @p whom.
     * @param whom  Receiver of the message.
     * @param rtime Relative time until this messages times out.
     * @param what  Message content as tuple.
     */
    untyped_response_handle timed_sync_send_tuple(const util::duration& rtime,
                                          const actor& whom,
                                          any_tuple what) {
        return {timed_sync_send_tuple_impl(message_priority::normal, whom,
                                           rtime, std::move(what)),
                this};
    }

    /**
     * @brief Sends a synchronous message with timeout @p rtime to @p whom.
     * @param whom  Receiver of the message.
     * @param rtime Relative time until this messages times out.
     * @param what  Message elements.
     * @pre <tt>sizeof...(Ts) > 0</tt>
     */
    template<typename... Ts>
    untyped_response_handle timed_sync_send(const actor& whom,
                                    const util::duration& rtime,
                                    Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return timed_sync_send_tuple(rtime, whom,
                                     make_any_tuple(std::forward<Ts>(what)...));
    }

};

} // namespace cppa

#endif // CPPA_UNTYPED_ACTOR_HPP
