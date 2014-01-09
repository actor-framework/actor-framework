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
#include "cppa/local_actor.hpp"
#include "cppa/mailbox_based.hpp"
#include "cppa/mailbox_element.hpp"

namespace cppa {

/**
 * @extends local_actor
 */
class blocking_untyped_actor : public extend<local_actor>::with<mailbox_based> {

 public:

    class response_future {

     public:

        response_future() = delete;

        void await(behavior&);

        inline void await(behavior&& bhvr) {
            behavior arg{std::move(bhvr)};
            await(arg);
        }

        /**
         * @brief Blocks until the response arrives and then executes @p mexpr.
         */
        template<typename... Cs, typename... Ts>
        void await(const match_expr<Cs...>& arg, const Ts&... args) {
            await(match_expr_convert(arg, args...));
        }

        /**
         * @brief Blocks until the response arrives and then executes @p @p fun,
         *        calls <tt>self->handle_sync_failure()</tt> if the response
         *        message is an 'EXITED' or 'VOID' message.
         */
        template<typename... Fs>
        typename std::enable_if<util::all_callable<Fs...>::value>::type
        await(Fs... fs) {
            await(behavior{(on_arg_match >> std::move(fs))...});
        }

        /**
         * @brief Returns the awaited response ID.
         */
        inline const message_id& id() const { return m_mid; }

        response_future(const response_future&) = default;
        response_future& operator=(const response_future&) = default;

        inline response_future(const message_id& from,
                               blocking_untyped_actor* self)
            : m_mid(from), m_self(self) { }

     private:

        message_id m_mid;
        blocking_untyped_actor* m_self;

    };

    class sync_receive_helper {

     public:

        inline sync_receive_helper(const response_future& mf) : m_mf(mf) { }

        template<typename... Ts>
        inline void operator()(Ts&&... args) {
            m_mf.await(std::forward<Ts>(args)...);
        }

     private:

        response_future m_mf;

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

    response_future timed_sync_send_tuple(const util::duration& rtime,
                                          const actor& dest,
                                          any_tuple what);

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

    template<typename... Ts>
    response_future timed_sync_send(const actor& dest,
                                   const util::duration& rtime,
                                   Ts&&... what) {
        static_assert(sizeof...(Ts) > 0, "no message to send");
        return timed_sync_send_tuple(rtime, dest, make_any_tuple(std::forward<Ts>(what)...));
    }

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
     * @brief Handles a synchronous response message in an event-based way.
     * @param handle A future for a synchronous response.
     * @throws std::logic_error if @p handle is not valid or if the actor
     *                          already received the response for @p handle
     * @relates response_future
     */
    inline sync_receive_helper receive_response(const response_future& f) {
        return {f};
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

    inline optional<behavior&> sync_handler(message_id msg_id) {
        auto i = m_sync_handler.find(msg_id);
        if (i != m_sync_handler.end()) return i->second;
        return none;
    }

    // unused in blocking actors
    inline void remove_handler(message_id) { }

    inline void dequeue(behavior&& bhvr) {
        behavior tmp{std::move(bhvr)};
        dequeue(tmp);
    }

    inline void dequeue(behavior& bhvr) {
        dequeue_response(bhvr, message_id::invalid);
    }

    virtual void dequeue_response(behavior& bhvr, message_id mid) = 0;

    virtual mailbox_element* dequeue() = 0;

    virtual mailbox_element* try_dequeue() = 0;

    virtual mailbox_element* try_dequeue(const timeout_type&) = 0;

    void await_all_other_actors_done();

    virtual void act() = 0;

 private:

    std::map<message_id, behavior> m_sync_handler;

    std::function<void(behavior&)> make_dequeue_callback() {
        return [=](behavior& bhvr) { dequeue(bhvr); };
    }

};

} // namespace cppa

#endif // CPPA_BLOCKING_UNTYPED_ACTOR_HPP
