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


#ifndef CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP
#define CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP

#include <tuple>
#include <stack>
#include <memory>
#include <vector>

#include "cppa/config.hpp"
#include "cppa/extend.hpp"
#include "cppa/behavior.hpp"
#include "cppa/scheduled_actor.hpp"
#include "cppa/detail/receive_policy.hpp"

namespace cppa {

/**
 * @brief Base class for all event-based actor implementations.
 */
class event_based_actor : public scheduled_actor {

    friend class detail::receive_policy;

    typedef scheduled_actor super;

 public:

    /**
     * @brief Finishes execution with exit reason
     *        {@link exit_reason::unallowed_function_call unallowed_function_call}.
     */
    void dequeue(behavior&);

    /**
     * @copydoc dequeue(behavior&)
     */
    void dequeue_response(behavior&, message_id);

    resume_result resume(util::fiber*, actor_ptr&);

    /**
     * @brief Initializes the actor.
     */
    virtual void init() = 0;

    virtual void quit(std::uint32_t reason = exit_reason::normal);

    bool has_behavior();

    scheduled_actor_type impl_type();

    static intrusive_ptr<event_based_actor> from(std::function<void()> fun);

 protected:

    event_based_actor(actor_state st = actor_state::blocked);

    // provoke compiler errors for usage of receive() and related functions

    /**
     * @brief Provokes a compiler error to ensure that an event-based actor
     *        does not accidently uses receive() instead of become().
     */
    template<typename... Ts>
    void receive(Ts&&...) {
        // this asssertion always fails
        static_assert((sizeof...(Ts) + 1) < 1,
                      "You shall not use receive in an event-based actor. "
                      "Use become() instead.");
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Ts>
    void receive_loop(Ts&&... args) {
        receive(std::forward<Ts>(args)...);
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Ts>
    void receive_while(Ts&&... args) {
        receive(std::forward<Ts>(args)...);
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Ts>
    void do_receive(Ts&&... args) {
        receive(std::forward<Ts>(args)...);
    }

    void do_become(behavior&& bhvr, bool discard_old);

    void become_waiting_for(behavior bhvr, message_id mf);

    detail::receive_policy m_recv_policy;

 private:

    inline bool has_behavior() const {
        return m_bhvr_stack.empty() == false;
    }

    inline behavior& get_behavior() {
        CPPA_REQUIRE(m_bhvr_stack.empty() == false);
        return m_bhvr_stack.back();
    }

    // required by detail::nestable_receive_policy
    static const detail::receive_policy_flag receive_flag = detail::rp_sequential;
    inline void handle_timeout(behavior& bhvr) {
        CPPA_REQUIRE(bhvr.timeout().valid());
        reset_timeout();
        bhvr.handle_timeout();
        if (m_bhvr_stack.empty() == false) {
            request_timeout(get_behavior().timeout());
        }
    }

};

} // namespace cppa

#endif // CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP
