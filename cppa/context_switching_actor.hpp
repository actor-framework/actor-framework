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


#ifndef CPPA_CONTEXT_SWITCHING_ACTOR_HPP
#define CPPA_CONTEXT_SWITCHING_ACTOR_HPP

#include "cppa/config.hpp"

#ifndef CPPA_DISABLE_CONTEXT_SWITCHING

#include <stack>

#include "cppa/either.hpp"
#include "cppa/pattern.hpp"

#include "cppa/detail/yield_interface.hpp"
#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/abstract_scheduled_actor.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Context-switching actor implementation.
 */
class context_switching_actor : public scheduled_actor {

 protected:

    /**
     * @brief Implements the actor's behavior.
     *        Reimplemented this function for a class-based context-switching
     *        actor. Returning from this member function will end the
     *        execution of the actor.
     */
    virtual void run();

};

#else

class context_switching_actor : public detail::abstract_scheduled_actor {

    friend class detail::receive_policy;

    typedef detail::abstract_scheduled_actor super;

 public:

    context_switching_actor(std::function<void()> fun);

    void dequeue(behavior& bhvr); //override

    void dequeue(partial_function& fun); //override

    resume_result resume(util::fiber* from); //override

 protected:

    context_switching_actor();

    virtual void run();

 private:

    // required by detail::nestable_receive_policy
    static const detail::receive_policy_flag receive_flag = detail::rp_nestable;
    detail::recursive_queue_node* receive_node();
    inline void push_timeout() { ++m_active_timeout_id; }
    inline void pop_timeout() { --m_active_timeout_id; }

    // required by util::fiber
    static void trampoline(void* _this);

    // members
    util::fiber m_fiber;
    std::function<void()> m_behavior;
    detail::receive_policy m_recv_policy;

};

#endif // CPPA_DOCUMENTATION

} // namespace cppa

#endif // CPPA_DISABLE_CONTEXT_SWITCHING

#endif // CPPA_CONTEXT_SWITCHING_ACTOR_HPP
