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

#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/behavior_stack.hpp"
#include "cppa/detail/yield_interface.hpp"
#include "cppa/detail/stacked_actor_mixin.hpp"
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
     *        Reimplemented this function for a class-based actor.
     *        Returning from this member function will end the
     *        execution of the actor.
     */
    virtual void run();

};

#else // CPPA_DOCUMENTATION

class context_switching_actor : public detail::stacked_actor_mixin<
                                           context_switching_actor,
                                           detail::abstract_scheduled_actor> {

    friend class detail::receive_policy;

    typedef detail::stacked_actor_mixin<
                context_switching_actor,
                detail::abstract_scheduled_actor> super;

 public:

    context_switching_actor(std::function<void()> fun);

    resume_result resume(util::fiber* from); //override

    scheduled_actor_type impl_type();

 protected:

    context_switching_actor();

 private:

    // required by detail::nestable_receive_policy
    static const detail::receive_policy_flag receive_flag = detail::rp_nestable;
    detail::recursive_queue_node* receive_node();
    inline int init_timeout(const util::duration& timeout) {
        // request timeout message
        request_timeout(timeout);
        return 0;
    }
    inline detail::recursive_queue_node* try_receive_node() {
        return m_mailbox.try_pop();
    }
    inline detail::recursive_queue_node* try_receive_node(int) {
        // timeout is triggered from an explicit timeout message
        return receive_node();
    }

    // required by util::fiber
    static void trampoline(void* _this);

    // members
    util::fiber m_fiber;

};

#endif // CPPA_DOCUMENTATION

} // namespace cppa

#endif // CPPA_DISABLE_CONTEXT_SWITCHING

#endif // CPPA_CONTEXT_SWITCHING_ACTOR_HPP
