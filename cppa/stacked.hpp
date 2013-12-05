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


#ifndef CPPA_STACKED_ACTOR_MIXIN_HPP
#define CPPA_STACKED_ACTOR_MIXIN_HPP

#include <chrono>
#include <memory>
#include <functional>

#include "cppa/behavior.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/mailbox_element.hpp"

#include "cppa/util/dptr.hpp"

#include "cppa/detail/receive_policy.hpp"

namespace cppa {

/**
 * @brief An actor that uses the blocking API of @p libcppa and thus needs
 *        its own stack.
 */
template<class Base, class Subtype>
class stacked : public Base {

    friend class detail::receive_policy;
    friend class detail::behavior_stack;

 protected:

    typedef stacked combined_type;

 public:

    static constexpr auto receive_flag = detail::rp_nestable;

    struct receive_while_helper {

        std::function<void(behavior&)> m_dq;
        std::function<bool()> m_stmt;

        template<typename... Ts>
        void operator()(Ts&&... args) {
            static_assert(sizeof...(Ts) > 0,
                          "operator() requires at least one argument");
            behavior bhvr = match_expr_convert(std::forward<Ts>(args)...);
            while (m_stmt()) m_dq(bhvr);
        }

    };

    template<typename T>
    struct receive_for_helper {
        
        std::function<void(behavior&)> m_dq;
        T& begin;
        T end;
        
        template<typename... Ts>
        void operator()(Ts&&... args) {
            behavior bhvr = match_expr_convert(std::forward<Ts>(args)...);
            for ( ; begin != end; ++begin) m_dq(bhvr);
        }
        
    };

    struct do_receive_helper {
        
        std::function<void(behavior&)> m_dq;
        behavior m_bhvr;
        
        template<typename Statement>
        void until(Statement stmt) {
            do { m_dq(m_bhvr); }
            while (stmt() == false);
        }
        
    };

    /**
     * @brief Dequeues the next message from the mailbox that
     *        is matched by given behavior.
     */
    template<typename... Ts>
    void receive(Ts&&... args) {
        static_assert(sizeof...(Ts), "receive() requires at least one argument");
        dequeue(match_expr_convert(std::forward<Ts>(args)...));
    }
    
    /**
     * @brief Receives messages in an endless loop.
     *        Semantically equal to: <tt>for (;;) { receive(...); }</tt>
     */
    template<typename... Ts>
    void receive_loop(Ts&&... args) {
        behavior bhvr = match_expr_convert(std::forward<Ts>(args)...);
        for (;;) dequeue(bhvr);
    }
    
    /**
     * @brief Receives messages as in a range-based loop.
     *
     * Semantically equal to:
     * <tt>for ( ; begin != end; ++begin) { receive(...); }</tt>.
     *
     * <b>Usage example:</b>
     * @code
     * int i = 0;
     * receive_for(i, 10) (
     *     on(atom("get")) >> [&]() -> any_tuple { return {"result", i}; }
     * );
     * @endcode
     * @param begin First value in range.
     * @param end Last value in range (excluded).
     * @returns A functor implementing the loop.
     */
    template<typename T>
    receive_for_helper<T> receive_for(T& begin, const T& end) {
        return {make_dequeue_callback(), begin, end};
    }
    
    /**
     * @brief Receives messages as long as @p stmt returns true.
     *
     * Semantically equal to: <tt>while (stmt()) { receive(...); }</tt>.
     *
     * <b>Usage example:</b>
     * @code
     * int i = 0;
     * receive_while([&]() { return (++i <= 10); })
     * (
     *     on<int>() >> int_fun,
     *     on<float>() >> float_fun
     * );
     * @endcode
     * @param stmt Lambda expression, functor or function returning a @c bool.
     * @returns A functor implementing the loop.
     */
    template<typename Statement>
    receive_while_helper receive_while(Statement stmt) {
        static_assert(std::is_same<bool, decltype(stmt())>::value,
                      "functor or function does not return a boolean");
        return {make_dequeue_callback(), stmt};
    }
    
    /**
     * @brief Receives messages until @p stmt returns true.
     *
     * Semantically equal to: <tt>do { receive(...); } while (stmt() == false);</tt>
     *
     * <b>Usage example:</b>
     * @code
     * int i = 0;
     * do_receive
     * (
     *     on<int>() >> int_fun,
     *     on<float>() >> float_fun
     * )
     * .until([&]() { return (++i >= 10); };
     * @endcode
     * @param bhvr Denotes the actor's response the next incoming message.
     * @returns A functor providing the @c until member function.
     */
    template<typename... Ts>
    do_receive_helper do_receive(Ts&&... args) {
        return { make_dequeue_callback()
               , match_expr_convert(std::forward<Ts>(args)...)};
    }

    virtual void run() {
        if (m_behavior) m_behavior();
        auto dthis = util::dptr<Subtype>(this);
        auto rsn = dthis->planned_exit_reason();
        dthis->cleanup(rsn == exit_reason::not_exited ? exit_reason::normal : rsn);
    }

    inline void set_behavior(std::function<void()> fun) {
        m_behavior = std::move(fun);
    }

    inline optional<behavior&> sync_handler(message_id msg_id) {
        auto i = m_sync_handler.find(msg_id);
        if (i != m_sync_handler.end()) return i->second;
        return none;
    }

 protected:

    template<typename... Ts>
    stacked(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

    virtual bool has_behavior() {
        return static_cast<bool>(m_behavior);
    }

    std::function<void()> m_behavior;
    detail::receive_policy m_recv_policy;

 private:

    std::map<message_id, behavior> m_sync_handler;

    std::function<void(behavior&)> make_dequeue_callback() {
        return [=](behavior& bhvr) { dequeue(bhvr); };
    }

    void dequeue(behavior& bhvr) {
        m_recv_policy.receive(util::dptr<Subtype>(this), bhvr);
    }

    void dequeue_response(behavior& bhvr, message_id request_id) {
        m_recv_policy.receive(util::dptr<Subtype>(this), bhvr, request_id);
    }

};

} // namespace cppa

#endif // CPPA_STACKED_ACTOR_MIXIN_HPP
