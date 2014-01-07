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
#include "cppa/behavior_stack_based.hpp"

namespace cppa {

class untyped_actor;

class continue_helper {

 public:

    typedef int message_id_wrapper_tag;

    inline continue_helper(message_id mid, untyped_actor* self)
        : m_mid(mid), m_self(self) { }

    template<typename F>
    continue_helper& continue_with(F fun) {
        return continue_with(behavior::continuation_fun{partial_function{
                   on(any_vals, arg_match) >> fun
               }});
    }

    continue_helper& continue_with(behavior::continuation_fun fun);

    message_id get_message_id() const {
        return m_mid;
    }

 private:

    message_id m_mid;
    untyped_actor* m_self;

};

/**
 * @extends local_actor
 */
class untyped_actor : public extend<local_actor>::with<mailbox_based,
                                                       behavior_stack_based> {

 protected:

    virtual behavior make_behavior() = 0;

    void forward_to(const actor& other);

 public:

    class response_future {

     public:

        response_future() = delete;

        inline continue_helper then(behavior bhvr) {
            m_self->bhvr_stack().push_back(std::move(bhvr), m_mid);
            return {m_mid, m_self};
        }

        /**
         * @brief Sets @p mexpr as event-handler for the response message.
         */
        template<typename... Cs, typename... Ts>
        continue_helper then(const match_expr<Cs...>& arg, const Ts&... args) {
            return then(match_expr_convert(arg, args...));
        }

        /**
         * @brief Sets @p fun as event-handler for the response message, calls
         *        <tt>self->handle_sync_failure()</tt> if the response message
         *        is an 'EXITED' or 'VOID' message.
         */
        template<typename... Fs>
        typename std::enable_if<
            util::all_callable<Fs...>::value,
            continue_helper
        >::type
        then(Fs... fs) {
            return then(behavior{(on_arg_match >> std::move(fs))...});
        }

        response_future(const response_future&) = default;

        response_future& operator=(const response_future&) = default;

        inline response_future(const message_id& from, untyped_actor* self)
            : m_mid(from), m_self(self) { }

     private:

        message_id m_mid;
        untyped_actor* m_self;

        inline void check_consistency() { }

    };

    /**
     * @brief Sends @p what as a synchronous message to @p whom.
     * @param whom Receiver of the message.
     * @param what Message content as tuple.
     * @returns A handle identifying a future to the response of @p whom.
     * @warning The returned handle is actor specific and the response to the
     *          sent message cannot be received by another actor.
     * @throws std::invalid_argument if <tt>whom == nullptr</tt>
     */
    response_future sync_send_tuple(const actor& dest, any_tuple what);


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
    inline response_future sync_send(const actor& dest, Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return sync_send_tuple(dest, make_any_tuple(std::forward<Ts>(what)...));
    }

    response_future timed_sync_send_tuple(const util::duration& rtime,
                                          const actor& dest,
                                          any_tuple what);

    template<typename... Ts>
    response_future timed_sync_send(const actor& dest,
                                   const util::duration& rtime,
                                   Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return timed_sync_send_tuple(rtime, dest, make_any_tuple(std::forward<Ts>(what)...));
    }

};

} // namespace cppa

#endif // CPPA_UNTYPED_ACTOR_HPP
