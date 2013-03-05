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

#include <stack>

#include "cppa/config.hpp"
#include "cppa/either.hpp"
#include "cppa/extend.hpp"
#include "cppa/stacked.hpp"
#include "cppa/scheduled_actor.hpp"


#include "cppa/detail/receive_policy.hpp"
#include "cppa/detail/behavior_stack.hpp"
#include "cppa/detail/yield_interface.hpp"

namespace cppa {

/**
 * @brief Context-switching actor implementation.
 * @extends scheduled_actor
 */
class context_switching_actor : public extend<scheduled_actor,context_switching_actor>::with<stacked> {

    friend class detail::behavior_stack;
    friend class detail::receive_policy;

    typedef combined_type super;

 public:

    /**
     * @brief Creates a context-switching actor running @p fun.
     */
    context_switching_actor(std::function<void()> fun);

    resume_result resume(util::fiber* from, actor_ptr& next_job); //override

    scheduled_actor_type impl_type();

 protected:

    timeout_type init_timeout(const util::duration& rel_time);

    detail::recursive_queue_node* await_message();

    detail::recursive_queue_node* await_message(const timeout_type& abs_time);

 private:

    detail::recursive_queue_node* receive_node();

    // required by util::fiber
    static void trampoline(void* _this);

    // members
    util::fiber m_fiber;

};

} // namespace cppa

#endif // CPPA_CONTEXT_SWITCHING_ACTOR_HPP
