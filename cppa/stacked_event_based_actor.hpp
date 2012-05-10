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


#ifndef STACKED_EVENT_BASED_ACTOR_HPP
#define STACKED_EVENT_BASED_ACTOR_HPP

#include "cppa/event_based_actor_base.hpp"

namespace cppa {

/**
 * @brief A base class for event-based actors using a behavior stack.
 */
class stacked_event_based_actor : public event_based_actor_base<stacked_event_based_actor>
{

    friend class event_based_actor_base<stacked_event_based_actor>;

    typedef abstract_event_based_actor::stack_element stack_element;

    void do_become(behavior* behavior, bool has_ownership);

 protected:

    /**
     * @brief Restores the last behavior.
     */
    void unbecome();

    /**
     * @brief Terminates this actor with normal exit reason.
     */
    void become_void();

};

} // namespace cppa

#endif // STACKED_EVENT_BASED_ACTOR_HPP
