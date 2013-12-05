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
#include "cppa/stackless.hpp"
#include "cppa/scheduled_actor.hpp"
#include "cppa/detail/receive_policy.hpp"

namespace cppa { namespace policy {

/**
 * @brief Base class for all event-based actor implementations.
 * @extends scheduled_actor
 */
class event_based_resume {

    friend class detail::receive_policy;

    typedef combined_type super;

 public:

    resume_result resume(untyped_actor*, util::fiber*);

    /**
     * @brief Initializes the actor.
     */
    virtual behavior make_behavior() = 0;

    scheduled_actor_type impl_type();

    static intrusive_ptr<event_based_actor> from(std::function<void()> fun);

    static intrusive_ptr<event_based_actor> from(std::function<behavior()> fun);

    static intrusive_ptr<event_based_actor> from(std::function<void(event_based_actor*)> fun);

    static intrusive_ptr<event_based_actor> from(std::function<behavior(event_based_actor*)> fun);

 protected:

    event_based_actor(actor_state st = actor_state::blocked);

};

} } // namespace cppa::policy

#endif // CPPA_ABSTRACT_EVENT_BASED_ACTOR_HPP
