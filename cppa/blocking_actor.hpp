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


#ifndef CPPA_BLOCKING_UNTYPED_ACTOR_HPP
#define CPPA_BLOCKING_UNTYPED_ACTOR_HPP

#include "cppa/on.hpp"
#include "cppa/extend.hpp"
#include "cppa/behavior.hpp"
#include "cppa/typed_actor.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/mailbox_based.hpp"
#include "cppa/mailbox_element.hpp"
#include "cppa/response_handle.hpp"

#include "cppa/detail/response_handle_util.hpp"

namespace cppa {

/**
 * @brief A thread-mapped or context-switching actor using a blocking
 *        receive rather than a behavior-stack based message processing.
 * @extends local_actor
 */
class blocking_actor : public extend<local_actor>::with<mailbox_based> {

 public:

    typedef blocking_response_handle_tag response_handle_tag;

    typedef response_handle<blocking_actor> response_handle_type;

    /**************************************************************************
     *                     sync_send[_tuple](actor, ...)                      *
     **************************************************************************/

    /**
     * @brief Sends @p what as a synchronous message to @p whom.
     * @param whom Receiver of the message.
     * @param what Message content as tuple.
     * @returns A handle identifying a future to the response of @p whom.
     * @warning The returned handle is actor specific and the response to the
     *          sent message cannot be received by another actor.
     * @throws std::invalid_argument if <tt>whom == nullptr</tt>
     */
    inline response_handle_type sync_send_tuple(message_priority prio,
                                           const actor& dest,
                                           any_tuple what) {
        return {sync_send_tuple_impl(prio, dest, std::move(what)), this};
    }

    inline response_handle_type sync_send_tuple(const actor& dest, any_tuple what) {
        return sync_send_tuple(message_priority::normal, dest, std::move(what));
    }

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
    inline response_handle_type sync_send(message_priority prio,
                                     const actor& dest, Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return sync_send_tuple(prio, dest,
                               make_any_tuple(std::forward<Ts>(what)...));
    }

    template<typename... Ts>
    inline response_handle_type sync_send(const actor& dest, Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return sync_send_tuple(message_priority::normal,
                               dest, make_any_tuple(std::forward<Ts>(what)...));
    }

    /**************************************************************************
     *                  timed_sync_send[_tuple](actor, ...)                   *
     **************************************************************************/


    inline response_handle_type timed_sync_send_tuple(const util::duration& rtime,
                                                 const actor& dest,
                                                 any_tuple what) {
        return {timed_sync_send_tuple_impl(message_priority::normal,
                                           dest, rtime, std::move(what)),
                this};
    }

    template<typename... Ts>
    response_handle_type timed_sync_send(const actor& dest,
                                   const util::duration& rtime,
                                   Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return timed_sync_send_tuple(rtime, dest,
                                     make_any_tuple(std::forward<Ts>(what)...));
    }

    /**************************************************************************
     *                sync_send[_tuple](typed_actor<...>, ...)                *
     **************************************************************************/

    template<typename... Rs, typename... Ts>
    inline response_handle_type sync_send_tuple(message_priority prio,
                                           const typed_actor<Rs...>& dest,
                                           cow_tuple<Ts...> what) {
        return {sync_send_tuple_impl(prio, dest, std::move(what)), this};
    }

    template<typename... Rs, typename... Ts>
    inline response_handle_type sync_send_tuple(const typed_actor<Rs...>& dest,
                                           cow_tuple<Ts...> what) {
        return sync_send_tuple(message_priority::normal, dest, std::move(what));
    }

    /**************************************************************************
     *           utility stuff and receive() member function family           *
     **************************************************************************/

    typedef std::chrono::high_resolution_clock::time_point timeout_type;

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
        static_assert(sizeof...(Ts), "at least one argument required");
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
     * Semantically equal to:
     * <tt>do { receive(...); } while (stmt() == false);</tt>
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

    optional<behavior&> sync_handler(message_id msg_id) override {
        auto i = m_sync_handler.find(msg_id);
        if (i != m_sync_handler.end()) return i->second;
        return none;
    }

    /**
     * @brief Blocks this actor until all other actors are done.
     */
    void await_all_other_actors_done();

    /**
     * @brief Implements the actor's behavior.
     */
    virtual void act() = 0;

    /**
     * @brief Unwinds the stack by throwing an actor_exited exception.
     * @throws actor_exited
     */
    virtual void quit(std::uint32_t reason = exit_reason::normal);

    /** @cond PRIVATE */

    // required from invoke_policy; unused in blocking actors
    inline void remove_handler(message_id) { }

    // required by receive() member function family
    inline void dequeue(behavior&& bhvr) {
        behavior tmp{std::move(bhvr)};
        dequeue(tmp);
    }

    // required by receive() member function family
    inline void dequeue(behavior& bhvr) {
        dequeue_response(bhvr, message_id::invalid);
    }

    // implemented by detail::proper_actor
    virtual void dequeue_response(behavior& bhvr, message_id mid) = 0;

    /** @endcond */

 private:

    // helper function to implement receive_(for|while) and do_receive
    std::function<void(behavior&)> make_dequeue_callback() {
        return [=](behavior& bhvr) { dequeue(bhvr); };
    }

    std::map<message_id, behavior> m_sync_handler;

};

} // namespace cppa

#endif // CPPA_BLOCKING_UNTYPED_ACTOR_HPP
