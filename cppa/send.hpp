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

#include "cppa/self.hpp"
#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/message_header.hpp"
#include "cppa/message_future.hpp"
#include "cppa/typed_actor_ptr.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

/**
 * @ingroup MessageHandling
 * @{
 */

/**
 * @brief Stores sender, receiver, and message priority.
 */
struct destination_header {
    channel_ptr receiver;
    message_priority priority;
    inline destination_header(const self_type& s)
    : receiver(s), priority(message_priority::normal) { }
    template<typename T>
    inline destination_header(T dest)
    : receiver(std::move(dest)), priority(message_priority::normal) { }
    inline destination_header(channel_ptr dest, message_priority prio)
    : receiver(std::move(dest)), priority(prio) { }
    inline destination_header(destination_header&& hdr)
    : receiver(std::move(hdr.receiver)), priority(hdr.priority) { }
};

/**
 * @brief Sends @p what to the receiver specified in @p hdr.
 */
inline void send_tuple(destination_header hdr, any_tuple what) {
    if (hdr.receiver == nullptr) return;
    auto s = self.get();
    message_header fhdr{s, std::move(hdr.receiver), hdr.priority};
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
void send(const typed_actor_ptr<Signatures...>& whom, Ts&&... what) {
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
    send(whom.unbox(), std::forward<Ts>(what)...);
}

/**
 * @brief Sends <tt>{what...}</tt> to the receiver specified in @p hdr.
 * @pre <tt>sizeof...(Ts) > 0</tt>
 */
template<typename... Ts>
inline void send(destination_header hdr, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    send_tuple(std::move(hdr), make_any_tuple(std::forward<Ts>(what)...));
}

/**
 * @brief Sends @p what to @p whom, but sets the sender information to @p from.
 */
inline void send_tuple_as(actor_ptr from, channel_ptr whom, any_tuple what) {
    message_header hdr{std::move(from), std::move(whom)};
    hdr.deliver(std::move(what));
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
inline void send_as(actor_ptr from, channel_ptr whom, Ts&&... what) {
    send_tuple_as(std::move(from), std::move(whom),
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
message_future sync_send_tuple(actor_ptr whom, any_tuple what);

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
inline message_future sync_send(actor_ptr whom, Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return sync_send_tuple(std::move(whom),
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
typed_message_future<
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
    return sync_send(whom.unbox(), std::forward<Ts>(what)...);
}

/**
 * @brief Sends a message to @p whom that is delayed by @p rel_time.
 * @param whom Receiver of the message.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param what Message content as a tuple.
 */
void delayed_send_tuple(const channel_ptr& to,
                        const util::duration& rel_time,
                        any_tuple data);

/**
 * @brief Sends a message to @p whom that is delayed by @p rel_time.
 * @param whom Receiver of the message.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param what Message content as a tuple.
 */
template<class Rep, class Period, typename... Ts>
inline void delayed_send_tuple(const channel_ptr& whom,
                               const std::chrono::duration<Rep, Period>& rtime,
                               any_tuple what) {
    delayed_send_tuple(whom, util::duration{rtime}, std::move(what));
}

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
void delayed_reply_tuple(const util::duration& rel_time,
                         actor_ptr whom,
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
 * @brief Sends a reply message that is delayed by @p rel_time.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param what Message content as a tuple.
 * @see delayed_send()
 */
template<class Rep, class Period, typename... Ts>
inline void delayed_reply_tuple(const std::chrono::duration<Rep, Period>& rtime,
                                any_tuple what) {
    delayed_reply_tuple(util::duration{rtime}, std::move(what));
}
/**
 * @brief Sends @p what as a synchronous message to @p whom with a timeout.
 *
 * The calling actor receives a 'TIMEOUT' message as response after
 * given timeout exceeded and no response messages was received.
 * @param whom Receiver of the message.
 * @param what Message content as tuple.
 * @returns A handle identifying a future to the response of @p whom.
 * @warning The returned handle is actor specific and the response to the sent
 *          message cannot be received by another actor.
 * @throws std::invalid_argument if <tt>whom == nullptr</tt>
 */
template<class Rep, class Period, typename... Ts>
message_future timed_sync_send_tuple(actor_ptr whom,
                                     const std::chrono::duration<Rep, Period>& rel_time,
                                     any_tuple what) {
    auto mf = sync_send_tuple(std::move(whom), std::move(what));
    auto tmp = make_any_tuple(atom("TIMEOUT"));
    delayed_reply_tuple(util::duration{rel_time}, self, mf.id(), std::move(tmp));
    return mf;
}

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
template<class Rep, class Period, typename... Ts>
message_future timed_sync_send(actor_ptr whom,
                               const std::chrono::duration<Rep, Period>& rel_time,
                               Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    return timed_sync_send_tuple(std::move(whom),
                                 rel_time,
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
inline void reply_to(const response_handle& handle, Ts&&... what) {
    if (handle.valid()) {
        handle.apply(make_any_tuple(std::forward<Ts>(what)...));
    }
}

/**
 * @brief Replies with @p what to @p handle.
 * @param handle Identifies a previously received request.
 * @param what Response message.
 */
inline void reply_tuple_to(const response_handle& handle, any_tuple what) {
    handle.apply(std::move(what));
}

/**
 * @brief Forwards the last received message to @p whom.
 */
inline void forward_to(const actor_ptr& whom) {
    self->forward_message(whom);
}

/**
 * @brief Sends a message to @p whom that is delayed by @p rel_time.
 * @param whom Receiver of the message.
 * @param rtime Relative time duration to delay the message in
 *              microseconds, milliseconds, seconds or minutes.
 * @param what Message elements.
 */
template<class Rep, class Period, typename... Ts>
inline void delayed_send(const channel_ptr& whom,
                         const std::chrono::duration<Rep, Period>& rtime,
                         Ts&&... what) {
    static_assert(sizeof...(Ts) > 0, "no message to send");
    if (whom) {
        delayed_send_tuple(whom,
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
template<class Rep, class Period, typename... Ts>
inline void delayed_reply(const std::chrono::duration<Rep, Period>& rtime,
                          Ts&&... what) {
    delayed_reply_tuple(rtime, make_any_tuple(std::forward<Ts>(what)...));
}

/**
 * @brief Sends an exit message to @p whom with @p reason.
 *
 * This function is syntactic sugar for
 * <tt>send(whom, atom("EXIT"), reason)</tt>.
 * @pre <tt>reason != exit_reason::normal</tt>
 */
inline void send_exit(actor_ptr whom, std::uint32_t rsn) {
    CPPA_REQUIRE(rsn != exit_reason::normal);
    send(std::move(whom), atom("EXIT"), rsn);
}

/**
 * @brief Sends an exit message to @p whom with @p reason.
 * @pre <tt>reason != exit_reason::normal</tt>
 */
template<typename... Signatures>
void send_exit(const typed_actor_ptr<Signatures...>& whom, std::uint32_t rsn) {
    send_exit(whom.unbox(), rsn);
}

/**
 * @}
 */

} // namespace cppa

#endif // CPPA_SEND_HPP
