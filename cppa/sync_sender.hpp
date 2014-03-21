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


#ifndef CPPA_SYNC_SENDER_HPP
#define CPPA_SYNC_SENDER_HPP

#include "cppa/actor.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/response_handle.hpp"
#include "cppa/message_priority.hpp"

#include "cppa/util/duration.hpp"

namespace cppa {

template<class Base, class Subtype, class ResponseHandleTag>
class sync_sender_impl : public Base {

 public:

    typedef response_handle<Subtype,
                            any_tuple,
                            ResponseHandleTag>
            response_handle_type;

    /**************************************************************************
     *                     sync_send[_tuple](actor, ...)                      *
     **************************************************************************/

    /**
     * @brief Sends @p what as a synchronous message to @p whom.
     * @param dest Receiver of the message.
     * @param what Message content as tuple.
     * @returns A handle identifying a future to the response of @p whom.
     * @warning The returned handle is actor specific and the response to the
     *          sent message cannot be received by another actor.
     * @throws std::invalid_argument if <tt>whom == nullptr</tt>
     */
    response_handle_type sync_send_tuple(message_priority prio,
                                         const actor& dest,
                                         any_tuple what) {
        return {dptr()->sync_send_tuple_impl(prio, dest, std::move(what)),
                dptr()};
    }

    response_handle_type sync_send_tuple(const actor& dest, any_tuple what) {
        return sync_send_tuple(message_priority::normal, dest, std::move(what));
    }

    /**
     * @brief Sends <tt>{what...}</tt> as a synchronous message to @p whom.
     * @param dest Receiver of the message.
     * @param what Message elements.
     * @returns A handle identifying a future to the response of @p whom.
     * @warning The returned handle is actor specific and the response to the
     *          sent message cannot be received by another actor.
     * @pre <tt>sizeof...(Ts) > 0</tt>
     * @throws std::invalid_argument if <tt>whom == nullptr</tt>
     */
    template<typename... Ts>
    response_handle_type sync_send(message_priority prio,
                                   const actor& dest,
                                   Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return sync_send_tuple(prio, dest,
                               make_any_tuple(std::forward<Ts>(what)...));
    }

    template<typename... Ts>
    response_handle_type sync_send(const actor& dest, Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return sync_send_tuple(message_priority::normal,
                               dest, make_any_tuple(std::forward<Ts>(what)...));
    }

    /**************************************************************************
     *                  timed_sync_send[_tuple](actor, ...)                   *
     **************************************************************************/

    response_handle_type timed_sync_send_tuple(message_priority prio,
                                               const actor& dest,
                                               const util::duration& rtime,
                                               any_tuple what) {
        return {dptr()->timed_sync_send_tuple_impl(prio, dest, rtime,
                                                   std::move(what)),
                dptr()};
    }

    response_handle_type timed_sync_send_tuple(const actor& dest,
                                               const util::duration& rtime,
                                               any_tuple what) {
        return {dptr()->timed_sync_send_tuple_impl(message_priority::normal,
                                                   dest, rtime,
                                                   std::move(what)),
                dptr()};
    }

    template<typename... Ts>
    response_handle_type timed_sync_send(message_priority prio,
                                         const actor& dest,
                                         const util::duration& rtime,
                                         Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return timed_sync_send_tuple(prio, dest, rtime,
                                     make_any_tuple(std::forward<Ts>(what)...));
    }

    template<typename... Ts>
    response_handle_type timed_sync_send(const actor& dest,
                                         const util::duration& rtime,
                                         Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return timed_sync_send_tuple(message_priority::normal, dest, rtime,
                                     make_any_tuple(std::forward<Ts>(what)...));
    }

    /**************************************************************************
     *                sync_send[_tuple](typed_actor<...>, ...)                *
     **************************************************************************/

    template<typename... Rs, typename... Ts>
    response_handle<
        Subtype,
        typename detail::deduce_output_type<
            util::type_list<Rs...>,
            util::type_list<Ts...>
        >::type,
        ResponseHandleTag
    >
    sync_send_tuple(message_priority prio,
                    const typed_actor<Rs...>& dest,
                    cow_tuple<Ts...> what) {
        return {dptr()->sync_send_tuple_impl(prio, dest, std::move(what)),
                dptr()};
    }

    template<typename... Rs, typename... Ts>
    response_handle<
        Subtype,
        typename detail::deduce_output_type<
            util::type_list<Rs...>,
            util::type_list<Ts...>
        >::type,
        ResponseHandleTag
    >
    sync_send_tuple(const typed_actor<Rs...>& dest, cow_tuple<Ts...> what) {
        return sync_send_tuple(message_priority::normal, dest, std::move(what));
    }

    template<typename... Rs, typename... Ts>
    response_handle<
        Subtype,
        typename detail::deduce_output_type<
            util::type_list<Rs...>,
            typename detail::implicit_conversions<
                typename util::rm_const_and_ref<
                    Ts
                >::type
            >::type...
        >::type,
        ResponseHandleTag
    >
    sync_send(message_priority prio,
              const typed_actor<Rs...>& dest,
              Ts&&... what) {
        return sync_send_tuple(prio, dest,
                               make_cow_tuple(std::forward<Ts>(what)...));
    }

    template<typename... Rs, typename... Ts>
    response_handle<
        Subtype,
        typename detail::deduce_output_type<
            util::type_list<Rs...>,
            util::type_list<
                typename detail::implicit_conversions<
                    typename util::rm_const_and_ref<
                        Ts
                    >::type
                >::type...
            >
        >::type,
        ResponseHandleTag
    >
    sync_send(const typed_actor<Rs...>& dest,
              Ts&&... what) {
        return sync_send_tuple(message_priority::normal, dest,
                               make_cow_tuple(std::forward<Ts>(what)...));
    }

    /**************************************************************************
     *             timed_sync_send[_tuple](typed_actor<...>, ...)             *
     **************************************************************************/

    template<typename... Rs, typename... Ts>
    response_handle<
        Subtype,
        typename detail::deduce_output_type<
            util::type_list<Rs...>,
            util::type_list<Ts...>
        >::type,
        ResponseHandleTag
    >
    timed_sync_send_tuple(message_priority prio,
                          const typed_actor<Rs...>& dest,
                          const util::duration& rtime,
                          cow_tuple<Ts...> what) {
        return {dptr()->timed_sync_send_tuple_impl(prio, dest, rtime,
                                                   std::move(what)),
                dptr()};
    }

    template<typename... Rs, typename... Ts>
    response_handle<
        Subtype,
        typename detail::deduce_output_type<
            util::type_list<Rs...>,
            util::type_list<Ts...>
        >::type,
        ResponseHandleTag
    >
    timed_sync_send_tuple(const typed_actor<Rs...>& dest,
                          const util::duration& rtime,
                          cow_tuple<Ts...> what) {
        return {dptr()->timed_sync_send_tuple_impl(message_priority::normal,
                                                   dest, rtime,
                                                   std::move(what)),
                dptr()};
    }

    template<typename... Rs, typename... Ts>
    response_handle<
        Subtype,
        typename detail::deduce_output_type<
            util::type_list<Rs...>,
            typename detail::implicit_conversions<
                typename util::rm_const_and_ref<
                    Ts
                >::type
            >::type...
        >::type,
        ResponseHandleTag
    >
    timed_sync_send(message_priority prio,
                    const typed_actor<Rs...>& dest,
                    const util::duration& rtime,
                    Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return timed_sync_send_tuple(prio, dest, rtime,
                                     make_any_tuple(std::forward<Ts>(what)...));
    }

    template<typename... Rs, typename... Ts>
    response_handle<
        Subtype,
        typename detail::deduce_output_type<
            util::type_list<Rs...>,
            typename detail::implicit_conversions<
                typename util::rm_const_and_ref<
                    Ts
                >::type
            >::type...
        >::type,
        ResponseHandleTag
    >
    timed_sync_send(const typed_actor<Rs...>& dest,
                    const util::duration& rtime,
                    Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return timed_sync_send_tuple(message_priority::normal, dest, rtime,
                                     make_any_tuple(std::forward<Ts>(what)...));
    }

 private:

    inline Subtype* dptr() {
        return static_cast<Subtype*>(this);
    }

};

template<class ResponseHandleTag>
class sync_sender {

 public:

    template<class Base, class Subtype>
    class impl : public sync_sender_impl<Base, Subtype, ResponseHandleTag> {

        typedef sync_sender_impl<Base, Subtype, ResponseHandleTag> super;

     public:

        typedef impl combined_type;

        template<typename... Ts>
        impl(Ts&&... args) : super(std::forward<Ts>(args)...) { }

    };

};

} // namespace cppa

#endif // CPPA_SYNC_SENDER_HPP
