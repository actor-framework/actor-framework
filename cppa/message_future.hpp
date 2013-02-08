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


#ifndef MESSAGE_FUTURE_HPP
#define MESSAGE_FUTURE_HPP

#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/behavior.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/message_id.hpp"
#include "cppa/local_actor.hpp"

namespace cppa {

/**
 * @brief Represents the result of a synchronous send.
 */
class message_future {

 public:

    class continue_helper {

     public:

        inline continue_helper(message_id_t mid) : m_mid(mid) { }

        template<typename F>
        void continue_with(F fun) {
            auto ref_opt = self->bhvr_stack().sync_handler(m_mid);
            if (ref_opt) {
                auto& ref = *ref_opt;
                // copy original behavior
                behavior cpy = ref;
                ref = cpy.add_continuation(std::move(fun));
            }
        }

     private:

        message_id_t m_mid;

    };

    message_future() = delete;

    /**
     * @brief Sets @p mexpr as event-handler for the response message.
     */
    template<typename... Cases, typename... Args>
    continue_helper then(const match_expr<Cases...>& arg0, const Args&... args) {
        consistency_check();
        self->become_waiting_for(match_expr_convert(arg0, args...), m_mid);
        return {m_mid};
    }

    /**
     * @brief Blocks until the response arrives and then executes @p mexpr.
     */
    template<typename... Cases, typename... Args>
    void await(const match_expr<Cases...>& arg0, const Args&... args) {
        consistency_check();
        self->dequeue_response(match_expr_convert(arg0, args...), m_mid);
    }

    /**
     * @brief Sets @p fun as event-handler for the response message, calls
     *        <tt>self->handle_sync_failure()</tt> if the response message
     *        is an 'EXITED', 'TIMEOUT', or 'VOID' message.
     */
    template<typename F>
    typename std::enable_if<util::is_callable<F>::value,continue_helper>::type
    then(F fun) {
        consistency_check();
        self->become_waiting_for(bhvr_from_fun(fun), m_mid);
        return {m_mid};
    }

    /**
     * @brief Blocks until the response arrives and then executes @p @p fun,
     *        calls <tt>self->handle_sync_failure()</tt> if the response
     *        message is an 'EXITED', 'TIMEOUT', or 'VOID' message.
     */
    template<typename F>
    typename std::enable_if<util::is_callable<F>::value>::type await(F fun) {
        consistency_check();
        self->dequeue_response(bhvr_from_fun(fun), m_mid);
    }

    /**
     * @brief Returns the awaited response ID.
     */
    inline const message_id_t& id() const { return m_mid; }

    message_future(const message_future&) = default;
    message_future& operator=(const message_future&) = default;

#   ifndef CPPA_DOCUMENTATION

    inline message_future(const message_id_t& from) : m_mid(from) { }

#   endif

 private:

    message_id_t m_mid;

    template<typename F>
    behavior bhvr_from_fun(F fun) {
        auto handle_sync_failure = [] { self->handle_sync_failure(); };
        return {
            on(atom("EXITED"), any_vals) >> handle_sync_failure,
            on(atom("TIMEOUT")) >> handle_sync_failure,
            on(atom("VOID")) >> handle_sync_failure,
            on(any_vals, arg_match) >> fun,
            others() >> handle_sync_failure
        };
    }

    void consistency_check() {
        if (!m_mid.valid() || !m_mid.is_response()) {
            throw std::logic_error("handle does not point to a response");
        }
        else if (!self->awaits(m_mid)) {
            throw std::logic_error("response already received");
        }
    }

};

} // namespace cppa

#endif // MESSAGE_FUTURE_HPP
