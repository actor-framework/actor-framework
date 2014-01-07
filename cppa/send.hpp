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


#ifndef CPPA_SEND_HPP
#define CPPA_SEND_HPP

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/message_header.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

/**
 * @ingroup MessageHandling
 * @{
 */

using channel_destination = destination_header<channel>;

using actor_destination = destination_header<abstract_actor_ptr>;

/**
 * @brief Sends @p what to the receiver specified in @p hdr.
 */
inline void send_tuple(channel_destination dest, any_tuple what) {
    if (dest.receiver == nullptr) return;
    auto s = self.get();
    message_header fhdr{s, std::move(dest.receiver), dest.priority};
    if (fhdr.receiver != s && s->chaining_enabled()) {
        if (fhdr.receiver->chained_enqueue(fhdr, std::move(what))) {
            // only actors implement chained_enqueue to return true
            s->chained_actor(fhdr.receiver.downcast<actor>());
        }
    }
    else fhdr.deliver(std::move(what));
}

/**
 * @brief Sends @p what to the receiver specified in @p hdr.
 */
template<typename... Signatures, typename... Ts>
void send(const typed_actor_ptr<Signatures...>& dest, Ts&&... what) {
    static constexpr int input_pos = util::tl_find_if<
                                         util::type_list<Signatures...>,
                                         detail::input_is<util::type_list<
                                            typename detail::implicit_conversions<
                                                typename util::rm_const_and_ref<
                                                    Ts
                                                >::type
                                            >::type...
                                         >>::template eval
                                     >::value;
    static_assert(input_pos >= 0, "typed actor does not support given input");
    send(dest./*receiver.*/type_erased(), std::forward<Ts>(what)...);
}

/**
 * @brief Sends <tt>{what...}</tt> to the receiver specified in @p hdr.
 * @pre <tt>sizeof...(Ts) > 0</tt>
 */
template<typename... Ts>
inline void send(channel_destination dest, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    send_tuple(std::move(dest), make_any_tuple(std::forward<Ts>(what)...));
}

/**
 * @brief Sends @p what to @p whom, but sets the sender information to @p from.
 */
inline void send_tuple_as(actor_ptr from, channel_destination dest, any_tuple what) {
    message_header mhdr{std::move(from), std::move(dest.receiver), dest.priority};
    mhdr.deliver(std::move(what));
}

/**
 * @brief Sends <tt>{what...}</tt> as a message to @p whom, but sets
 *        the sender information to @p from.
 * @param from Sender as seen by @p whom.
 * @param whom Receiver of the message.
 * @param what Message elements.
 * @pre <tt>sizeof...(Ts) > 0</tt>
 */
template<typename... Ts>
inline void send_as(actor_ptr from, channel_destination dest, Ts&&... what) {
    send_tuple_as(std::move(from), std::move(dest),
                  make_any_tuple(std::forward<Ts>(what)...));
}

/**
 * @brief Sends @p what as a synchronous message to @p whom.
 * @param whom Receiver of the message.
 * @param what Message content as tuple.
 * @returns A handle identifying a future to the response of @p whom.
 * @warning The returned handle is actor specific and the response to the sent
 *          message cannot be received by another actor.
 * @throws std::invalid_argument if <tt>whom == nullptr</tt>
 */
response_future sync_send_tuple(actor_destination dest, any_tuple what);

/**
 * @brief Sends <tt>{what...}</tt> as a synchronous message to @p whom.
 * @param whom Receiver of the message.
 * @param what Message elements.
 * @returns A handle identifying a future to the response of @p whom.
 * @warning The returned handle is actor specific and the response to the sent
 *          message cannot be received by another actor.
 * @pre <tt>sizeof...(Ts) > 0</tt>
 * @throws std::invalid_argument if <tt>whom == nullptr</tt>
 */
template<typename... Ts>
inline response_future sync_send(actor_destination dest, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return sync_send_tuple(std::move(dest),
                           make_any_tuple(std::forward<Ts>(what)...));
}

/**
 * @brief Sends <tt>{what...}</tt> as a synchronous message to @p whom.
 * @param whom Receiver of the message.
 * @param what Message elements.
 * @returns A handle identifying a future to the response of @p whom.
 * @warning The returned handle is actor specific and the response to the sent
 *          message cannot be received by another actor.
 * @pre <tt>sizeof...(Ts) > 0</tt>
 * @throws std::invalid_argument if <tt>whom == nullptr</tt>
 */
template<typename... Signatures, typename... Ts>
typed_response_future<
    typename detail::deduce_output_type<
        util::type_list<Signatures...>,
        util::type_list<
            typename detail::implicit_conversions<
                typename util::rm_const_and_ref<
                    Ts
                >::type
            >::type...
        >
    >::type
>
sync_send(const typed_actor_ptr<Signatures...>& whom, Ts&&... what) {
    return sync_send(whom.type_erased(), std::forward<Ts>(what)...);
}

/**
 * @brief Sends a message to @p whom that is delayed by @p rel_time.
 * @param whom Receiver of the message.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param data Message content as a tuple.
 */
void delayed_send_tuple(channel_destination dest,
                        const util::duration& rtime,
                        any_tuple data);

/**
 * @brief Sends a reply message that is delayed by @p rel_time.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param what Message content as a tuple.
 * @see delayed_send()
 */
void delayed_reply_tuple(const util::duration& rel_time,
                         message_id mid,
                         any_tuple data);

/**
 * @brief Sends a reply message that is delayed by @p rel_time.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param what Message content as a tuple.
 * @see delayed_send()
 */
void delayed_reply_tuple(const util::duration& rel_time, any_tuple data);


/**
 * @brief Sends @p what as a synchronous message to @p whom with a timeout.
 *
 * The calling actor receives a 'TIMEOUT' message as response after
 * given timeout exceeded and no response messages was received.
 * @param dest Receiver and optional priority flag.
 * @param what Message content as tuple.
 * @returns A handle identifying a future to the response of @p whom.
 * @warning The returned handle is actor specific and the response to the sent
 *          message cannot be received by another actor.
 * @throws std::invalid_argument if <tt>whom == nullptr</tt>
 */
response_future timed_sync_send_tuple(actor_destination dest,
                                     const util::duration& rtime,
                                     any_tuple what);

/**
 * @brief Sends <tt>{what...}</tt> as a synchronous message to @p whom
 *        with a timeout.
 *
 * The calling actor receives a 'TIMEOUT' message as response after
 * given timeout exceeded and no response messages was received.
 * @param whom Receiver of the message.
 * @param what Message elements.
 * @returns A handle identifying a future to the response of @p whom.
 * @warning The returned handle is actor specific and the response to the sent
 *          message cannot be received by another actor.
 * @pre <tt>sizeof...(Ts) > 0</tt>
 * @throws std::invalid_argument if <tt>whom == nullptr</tt>
 */
template<typename... Ts>
response_future timed_sync_send(actor_destination whom,
                               const util::duration& rtime,
                               Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return timed_sync_send_tuple(std::move(whom),
                                 rtime,
                                 make_any_tuple(std::forward<Ts>(what)...));
}

/**
 * @brief Sends a message to the sender of the last received message.
 * @param what Message content as a tuple.
 */
CPPA_DEPRECATED inline void reply_tuple(any_tuple what) {
    self->reply_message(std::move(what));
}

/**
 * @brief Sends a message to the sender of the last received message.
 * @param what Message elements.
 */
template<typename... Ts>
CPPA_DEPRECATED inline void reply(Ts&&... what) {
    self->reply_message(make_any_tuple(std::forward<Ts>(what)...));
}

/**
 * @brief Sends a message as reply to @p handle.
 */
template<typename... Ts>
inline void reply_to(const response_promise& handle, Ts&&... what) {
    if (handle.valid()) {
        handle.apply(make_any_tuple(std::forward<Ts>(what)...));
    }
}

/**
 * @brief Replies with @p what to @p handle.
 * @param handle Identifies a previously received request.
 * @param what Response message.
 */
inline void reply_tuple_to(const response_promise& handle, any_tuple what) {
    handle.apply(std::move(what));
}

/**
 * @brief Forwards the last received message to @p whom.
 */
inline void forward_to(actor_destination dest) {
    self->forward_message(dest.receiver, dest.priority);
}

/**
 * @brief Sends a message to @p whom that is delayed by @p rel_time.
 * @param whom Receiver of the message.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param what Message elements.
 */
template<typename... Ts>
inline void delayed_send(channel_destination dest,
                         const util::duration& rtime,
                         Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    if (dest.receiver) {
        delayed_send_tuple(std::move(dest),
                           rtime,
                           make_any_tuple(std::forward<Ts>(what)...));
    }
}

/**
 * @brief Sends a reply message that is delayed by @p rel_time.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param what Message elements.
 * @see delayed_send()
 */
template<typename... Ts>
inline void delayed_reply(const util::duration& rtime, Ts&&... what) {
    delayed_reply_tuple(rtime, make_any_tuple(std::forward<Ts>(what)...));
}

/**
 * @brief Sends an exit message to @p whom with @p reason.
 *
 * This function is syntactic sugar for
 * <tt>send(whom, atom("EXIT"), reason)</tt>.
 * @pre <tt>reason != exit_reason::normal</tt>
 */
inline void send_exit(channel_destination dest, std::uint32_t rsn) {
    CPPA_REQUIRE(rsn != exit_reason::normal);
    send(std::move(dest), atom("EXIT"), rsn);
}

/**
 * @brief Sends an exit message to @p whom with @p reason.
 * @pre <tt>reason != exit_reason::normal</tt>
 */
template<typename... Signatures>
void send_exit(const typed_actor_ptr<Signatures...>& whom, std::uint32_t rsn) {
    send_exit(whom.type_erased(), rsn);
}

/**
 * @}
 */

} // namespace cppa

#endif // CPPA_SEND_HPP
