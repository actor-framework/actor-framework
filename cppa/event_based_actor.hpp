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


#ifndef CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP
#define CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP

#include <tuple>
#include <stack>
#include <memory>
#include <vector>

#include "cppa/config.hpp"
#include "cppa/either.hpp"
#include "cppa/pattern.hpp"
#include "cppa/behavior.hpp"
#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/behavior_stack.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION
/**
 * @brief Base class for all event-based actor implementations.
 */
class event_based_actor : public scheduled_actor {
#else // CPPA_DOCUMENTATION
class event_based_actor : public detail::abstract_scheduled_actor {
#endif // CPPA_DOCUMENTATION

    friend class detail::receive_policy;
    typedef detail::abstract_scheduled_actor super;

 public:

    /**
     * @brief Finishes execution with exit reason
     *        {@link exit_reason::unallowed_function_call unallowed_function_call}.
     */
    void dequeue(behavior&); //override

    /**
     * @copydoc dequeue(behavior&)
     */
    void dequeue(partial_function&); //override

    resume_result resume(util::fiber*); //override

    /**
     * @brief Initializes the actor.
     */
    virtual void init() = 0;

    void quit(std::uint32_t reason = exit_reason::normal);

    void unbecome();

    bool has_behavior();

    scheduled_actor_type impl_type();

 protected:

    event_based_actor();

    // provoke compiler errors for usage of receive() and related functions

    /**
     * @brief Provokes a compiler error to ensure that an event-based actor
     *        does not accidently uses receive() instead of become().
     */
    template<typename... Args>
    void receive(Args&&...) {
        static_assert((sizeof...(Args) + 1) < 1,
                      "You shall not use receive in an event-based actor. "
                      "Use become() instead.");
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Args>
    void receive_loop(Args&&... args) {
        receive(std::forward<Args>(args)...);
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Args>
    void receive_while(Args&&... args) {
        receive(std::forward<Args>(args)...);
    }

    /**
     * @brief Provokes a compiler error.
     */
    template<typename... Args>
    void do_receive(Args&&... args) {
        receive(std::forward<Args>(args)...);
    }

    void do_become(behavior* bhvr, bool owns_bhvr, bool discard_old);

 private:

    inline behavior& get_behavior() {
        CPPA_REQUIRE(m_bhvr_stack.empty() == false);
        return m_bhvr_stack.back();
    }

        // required by detail::nestable_receive_policy
    static const detail::receive_policy_flag receive_flag = detail::rp_callback;
    inline void handle_timeout(behavior& bhvr) {
        CPPA_REQUIRE(bhvr.timeout().valid());
        m_has_pending_timeout_request = false;
        bhvr.handle_timeout();
        if (m_bhvr_stack.empty() == false) {
            request_timeout(get_behavior().timeout());
        }
    }

    // stack elements are moved to m_erased_stack_elements and erased later
    // to prevent possible segfaults that can occur if a currently executed
    // lambda gets deleted
    detail::behavior_stack m_bhvr_stack;
    detail::receive_policy m_policy;

};

} // namespace cppa

#endif // CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP
