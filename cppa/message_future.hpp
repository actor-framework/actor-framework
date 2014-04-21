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


#ifndef CPPA_MESSAGE_FUTURE_HPP
#define CPPA_MESSAGE_FUTURE_HPP

#include <cstdint>
#include <type_traits>

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/behavior.hpp"
#include "cppa/match_expr.hpp"
#include "cppa/message_id.hpp"

#include "cppa/util/type_traits.hpp"

#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

namespace detail { struct optional_any_tuple_visitor; }

/**
 * @brief Provides the @p continue_with member function as used in
 *        <tt>sync_send(...).then(...).continue_with(...)</tt>.
 */
class continue_helper {

 public:

    typedef int message_id_wrapper_tag;

    inline continue_helper(message_id mid) : m_mid(mid) { }

    template<typename F>
    continue_helper& continue_with(F fun) {
        return continue_with(behavior::continuation_fun{partial_function{
            on(any_vals, arg_match) >> fun
        }});
    }

    continue_helper& continue_with(behavior::continuation_fun fun);

    inline message_id get_message_id() const {
        return m_mid;
    }

 private:

    message_id m_mid;

};

/**
 * @brief Represents the result of a synchronous send.
 */
class message_future {

 public:

    message_future() = delete;

    /**
     * @brief Sets @c bhvr as event-handler for the response message.
     */
    continue_helper then(behavior bhvr) const;

    /**
     * @brief Sets @p mexpr as event-handler for the response message.
     */
    template<typename... Cs, typename... Ts>
    continue_helper then(const match_expr<Cs...>& arg, const Ts&... args) const {
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
    then(Fs... fs) const {
        return then(fs2bhvr(std::move(fs)...));
    }

    /**
     * @brief Blocks until the response arrives and then executes @p bhvr.
     */
    void await(behavior bhvr) const;

    /**
     * @brief Blocks until the response arrives and then executes @p mexpr.
     */
    template<typename... Cs, typename... Ts>
    void await(const match_expr<Cs...>& arg, const Ts&... args) const {
        await(match_expr_convert(arg, args...));
    }

    /**
     * @brief Blocks until the response arrives and then executes @p @p fun,
     *        calls <tt>self->handle_sync_failure()</tt> if the response
     *        message is an 'EXITED' or 'VOID' message.
     */
    template<typename... Fs>
    typename std::enable_if<util::all_callable<Fs...>::value>::type
    await(Fs... fs) const {
        await(fs2bhvr(std::move(fs)...));
    }

    /**
     * @brief Returns the awaited response ID.
     */
    inline const message_id& id() const { return m_mid; }

    message_future(const message_future&) = default;
    message_future& operator=(const message_future&) = default;

    inline message_future(const message_id& from) : m_mid(from) { }

 private:

    message_id m_mid;

    static match_hint handle_sync_timeout();

    template<typename... Fs>
    behavior fs2bhvr(Fs... fs) const {
        return {
            on(atom("EXITED"), val<std::uint32_t>) >> skip_message,
            on(atom("VOID")) >> skip_message,
            on(atom("TIMEOUT")) >> &message_future::handle_sync_timeout,
            (on(any_vals, arg_match) >> fs)...
        };
    }

    void check_consistency() const;

};

template<typename R>
class typed_continue_helper {

 public:

    typedef int message_id_wrapper_tag;

    typedef typename detail::lifted_result_type<R>::type result_types;

    typed_continue_helper(continue_helper ch) : m_ch(std::move(ch)) { }

    template<typename F>
    typed_continue_helper<typename util::get_callable_trait<F>::result_type>
    continue_with(F fun) {
        detail::assert_types<result_types, F>();
        m_ch.continue_with(std::move(fun));
        return {m_ch};
    }

    inline message_id get_message_id() const {
        return m_ch.get_message_id();
    }

 private:

    continue_helper m_ch;

};

template<typename OutputList>
class typed_message_future {

 public:

    typed_message_future(message_future&& mf) : m_mf(std::move(mf)) { }

    template<typename F>
    void await(F fun) {
        detail::assert_types<OutputList, F>();
        m_mf.await(fun);
    }

    template<typename F>
    typed_continue_helper<
        typename util::get_callable_trait<F>::result_type
    >
    then(F fun) {
        detail::assert_types<OutputList, F>();
        return m_mf.then(fun);
    }

 private:

    message_future m_mf;

};

class sync_handle_helper {

 public:

    inline sync_handle_helper(const message_future& mf) : m_mf(mf) { }

    template<typename... Ts>
    inline continue_helper operator()(Ts&&... args) {
        return m_mf.then(std::forward<Ts>(args)...);
    }

 private:

    message_future m_mf;

};

class sync_receive_helper {

 public:

    inline sync_receive_helper(const message_future& mf) : m_mf(mf) { }

    template<typename... Ts>
    inline void operator()(Ts&&... args) {
        m_mf.await(std::forward<Ts>(args)...);
    }

 private:

    message_future m_mf;

};

/**
 * @brief Receives a synchronous response message.
 * @param handle A future for a synchronous response.
 * @throws std::logic_error if @p handle is not valid or if the actor
 *                          already received the response for @p handle
 * @relates message_future
 */
inline sync_handle_helper handle_response(const message_future& f) {
    return {f};
}

/**
 * @brief Handles a synchronous response message in an event-based way.
 * @param handle A future for a synchronous response.
 * @throws std::logic_error if @p handle is not valid or if the actor
 *                          already received the response for @p handle
 * @relates message_future
 */
inline sync_receive_helper receive_response(const message_future& f) {
    return {f};
}

} // namespace cppa

#endif // CPPA_MESSAGE_FUTURE_HPP
