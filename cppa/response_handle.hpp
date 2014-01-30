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

#include <type_traits>

#include "cppa/message_id.hpp"
#include "cppa/typed_behavior.hpp"
#include "cppa/continue_helper.hpp"

#include "cppa/util/type_list.hpp"

#include "cppa/detail/response_handle_util.hpp"

namespace cppa {

/**
 * @brief This tag identifies response handles featuring a
 *        nonblocking API by providing a @p then member function.
 */
struct nonblocking_response_handle_tag { };

/**
 * @brief This tag identifies response handles featuring a
 *        blocking API by providing an @p await member function.
 */
struct blocking_response_handle_tag { };

/**
 * @brief This helper class identifies an expected response message
 *        and enables <tt>sync_send(...).then(...)</tt>.
 *
 * @tparam Self The type of the actor this handle belongs to.
 * @tparam Result Either any_tuple or type_list<R1, R2, ... Rn>.
 * @tparam Tag Either {@link nonblocking_response_handle_tag} or
 *             {@link blocking_response_handle_tag}.
 */
template<class Self,
         class Result = any_tuple,
         class Tag = typename Self::response_handle_tag>
class response_handle { // default impl for nonblocking_response_handle_tag

    friend Self;

 public:

    response_handle() = delete;

    response_handle(const response_handle&) = default;

    response_handle& operator=(const response_handle&) = default;

    /**
     * @brief Sets @p bhvr as event-handler for the response message.
     */
    inline continue_helper then(behavior bhvr) {
        return then_impl<Result>(std::move(bhvr));
    }

    /**
     * @brief Sets @p mexpr as event-handler for the response message.
     */
    template<typename... Cs, typename... Ts>
    continue_helper then(const match_expr<Cs...>& arg, const Ts&... args) {
        return then_impl<Result>({arg, args...});
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
        return then_impl<Result>(detail::fs2bhvr(m_self, fs...));
    }

 private:

    template<typename R>
    typename std::enable_if<
        std::is_same<R, any_tuple>::value,
        continue_helper
    >::type
    then_impl(behavior&& bhvr) {
        m_self->bhvr_stack().push_back(std::move(bhvr), m_mid);
        return {m_mid, m_self};
    }

    template<typename R>
    typename std::enable_if<
        util::is_type_list<R>::value,
        continue_helper
    >::type
    then_impl(behavior&& bhvr) {
        m_self->bhvr_stack().push_back(std::move(bhvr), m_mid);
        return {m_mid, m_self};
    }

    response_handle(const message_id& mid, Self* self)
            : m_mid(mid), m_self(self) { }

    message_id m_mid;

    Self* m_self;

};

template<class Self, typename Result>
class response_handle<Self, Result, blocking_response_handle_tag> {

    friend Self;

 public:

    response_handle() = delete;

    response_handle(const response_handle&) = default;

    response_handle& operator=(const response_handle&) = default;

    /**
     * @brief Blocks until the response arrives and then executes @p pfun.
     */
    void await(behavior& pfun) {
        m_self->dequeue_response(pfun, m_mid);
    }

    /**
     * @copydoc await(behavior&)
     */
    inline void await(behavior&& pfun) {
        behavior arg{std::move(pfun)};
        await(arg);
    }

    /**
     * @brief Blocks until the response arrives and then executes
     *        the corresponding handler.
     */
    template<typename... Cs, typename... Ts>
    void await(const match_expr<Cs...>& arg, const Ts&... args) {
        await(match_expr_convert(arg, args...));
    }

    /**
     * @brief Blocks until the response arrives and then executes
     *        the corresponding handler.
     * @note Calls <tt>self->handle_sync_failure()</tt> if the response
     *       message is an 'EXITED' or 'VOID' message.
     */
    template<typename... Fs>
    typename std::enable_if<util::all_callable<Fs...>::value>::type
    await(Fs... fs) {
        await(detail::fs2bhvr(m_self, fs...));
    }

 private:

    response_handle(const message_id& mid, Self* self)
            : m_mid(mid), m_self(self) { }

    message_id m_mid;

    Self* m_self;

};

} // namespace cppa

#endif // CPPA_RESPONSE_HANDLE_HPP
