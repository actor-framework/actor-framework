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
#include "cppa/typed_continue_helper.hpp"

#include "cppa/util/type_list.hpp"

#include "cppa/detail/typed_actor_util.hpp"
#include "cppa/detail/response_handle_util.hpp"

namespace cppa {

/**
 * @brief This tag identifies response handles featuring a
 *        nonblocking API by providing a @p then member function.
 * @relates response_handle
 */
struct nonblocking_response_handle_tag { };

/**
 * @brief This tag identifies response handles featuring a
 *        blocking API by providing an @p await member function.
 * @relates response_handle
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
template<class Self, class Result, class Tag>
class response_handle;

/******************************************************************************
 *                           nonblocking + untyped                            *
 ******************************************************************************/
template<class Self>
class response_handle<Self, any_tuple, nonblocking_response_handle_tag> {

 public:

    response_handle() = delete;

    response_handle(const response_handle&) = default;

    response_handle& operator=(const response_handle&) = default;

    inline continue_helper then(behavior bhvr) const {
        m_self->bhvr_stack().push_back(std::move(bhvr), m_mid);
        return {m_mid, m_self};
    }

    template<typename... Cs, typename... Ts>
    continue_helper then(const match_expr<Cs...>& arg, const Ts&... args) const {
        return then(behavior{arg, args...});
    }

    template<typename... Fs>
    typename std::enable_if<
        util::all_callable<Fs...>::value,
        continue_helper
    >::type
    then(Fs... fs) const {
        return then(detail::fs2bhvr(m_self, fs...));
    }

    response_handle(const message_id& mid, Self* self)
            : m_mid(mid), m_self(self) { }

 private:

    message_id m_mid;

    Self* m_self;

};

/******************************************************************************
 *                            nonblocking + typed                             *
 ******************************************************************************/
template<class Self, typename... Ts>
class response_handle<Self, util::type_list<Ts...>, nonblocking_response_handle_tag> {

 public:

    response_handle() = delete;

    response_handle(const response_handle&) = default;

    response_handle& operator=(const response_handle&) = default;

    template<typename F,
             typename Enable = typename std::enable_if<
                                      util::is_callable<F>::value
                                   && !is_match_expr<F>::value
                               >::type>
    typed_continue_helper<
        typename detail::lifted_result_type<
            typename util::get_callable_trait<F>::result_type
        >::type
    >
    then(F fun) {
        detail::assert_types<util::type_list<Ts...>, F>();
        m_self->bhvr_stack().push_back(behavior{on_arg_match >> fun}, m_mid);
        return {m_mid, m_self};
    }

    response_handle(const message_id& mid, Self* self)
            : m_mid(mid), m_self(self) { }

 private:

    message_id m_mid;

    Self* m_self;

};

/******************************************************************************
 *                             blocking + untyped                             *
 ******************************************************************************/
template<class Self>
class response_handle<Self, any_tuple, blocking_response_handle_tag> {

 public:

    response_handle() = delete;

    response_handle(const response_handle&) = default;

    response_handle& operator=(const response_handle&) = default;

    void await(behavior& pfun) {
        m_self->dequeue_response(pfun, m_mid);
    }

    inline void await(behavior&& pfun) {
        behavior tmp{std::move(pfun)};
        await(tmp);
    }

    template<typename... Cs, typename... Ts>
    void await(const match_expr<Cs...>& arg, const Ts&... args) {
        behavior bhvr{arg, args...};
        await(bhvr);
    }

    template<typename... Fs>
    typename std::enable_if<util::all_callable<Fs...>::value>::type
    await(Fs... fs) {
        await(detail::fs2bhvr(m_self, fs...));
    }

    response_handle(const message_id& mid, Self* self)
            : m_mid(mid), m_self(self) { }

 private:

   message_id m_mid;

   Self* m_self;

};

/******************************************************************************
 *                              blocking + typed                              *
 ******************************************************************************/
template<class Self, typename... Ts>
class response_handle<Self, util::type_list<Ts...>, blocking_response_handle_tag> {

 public:

    typedef util::type_list<Ts...> result_types;

    response_handle() = delete;

    response_handle(const response_handle&) = default;

    response_handle& operator=(const response_handle&) = default;

    template<typename F>
    void await(F fun) {
        typedef typename util::tl_map<
                    typename util::get_callable_trait<F>::arg_types,
                    util::rm_const_and_ref
                >::type
                arg_types;
        static constexpr size_t fun_args = util::tl_size<arg_types>::value;
        static_assert(fun_args <= util::tl_size<result_types>::value,
                      "functor takes too much arguments");
        typedef typename util::tl_right<result_types, fun_args>::type recv_types;
        static_assert(std::is_same<arg_types, recv_types>::value,
                      "wrong functor signature");
        behavior tmp = detail::fs2bhvr(m_self, fun);
        m_self->dequeue_response(tmp, m_mid);
    }

    response_handle(const message_id& mid, Self* self)
            : m_mid(mid), m_self(self) { }

 private:

    message_id m_mid;

    Self* m_self;

};

} // namespace cppa

#endif // CPPA_RESPONSE_HANDLE_HPP
