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

    message_future() = delete;

    /**
     * @brief Sets @p mexpr as event-handler for the response message.
     */
    template<typename... Expression>
    void then(Expression&&... mexpr) {
        auto f = [](behavior& bhvr, message_id_t mid) {
            self->become_waiting_for(std::move(bhvr), mid);
        };
        apply(f, std::forward<Expression>(mexpr)...);
    }

    /**
     * @brief Blocks until the response arrives and then executes @p mexpr.
     */
    template<typename... Expression>
    void await(Expression&&... mexpr) {
        auto f = [](behavior& bhvr, message_id_t mid) {
            self->dequeue_response(bhvr, mid);
        };
        apply(f, std::forward<Expression>(mexpr)...);
    }

    /**
     * @brief Returns the awaited response ID.
     */
    inline const message_id_t& id() const { return m_id; }

    message_future(const message_future&) = default;
    message_future& operator=(const message_future&) = default;

#   ifndef CPPA_DOCUMENTATION

    inline message_future(const message_id_t& from) : m_id(from) { }

#   endif

 private:

    message_id_t m_id;

    template<typename Fun, typename... Args>
    void apply(Fun& fun, Args&&... args) {
        auto bhvr = match_expr_convert(std::forward<Args>(args)...);
        static_assert(std::is_same<decltype(bhvr), behavior>::value,
                      "no timeout specified");
        if (bhvr.timeout().valid() == false || bhvr.timeout().is_zero()) {
            throw std::invalid_argument("specified timeout is invalid or zero");
        }
        else if (!m_id.valid() || !m_id.is_response()) {
            throw std::logic_error("handle does not point to a response");
        }
        else if (!self->awaits(m_id)) {
            throw std::logic_error("response already received");
        }
        fun(bhvr, m_id);
    }

};

} // namespace cppa

#endif // MESSAGE_FUTURE_HPP
