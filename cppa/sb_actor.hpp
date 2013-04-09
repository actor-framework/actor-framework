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


#ifndef CPPA_FSM_ACTOR_HPP
#define CPPA_FSM_ACTOR_HPP

#include <utility>
#include <type_traits>

#include "cppa/event_based_actor.hpp"

namespace cppa {

/**
 * @brief A base class for state-based actors using the
 *        Curiously Recurring Template Pattern
 *        to initialize the derived actor with its @p init_state member.
 * @tparam Derived Direct subclass of @p sb_actor.
 */
template<class Derived, class Base = event_based_actor>
class sb_actor : public Base {

    static_assert(std::is_base_of<event_based_actor,Base>::value,
                  "Base must be either event_based_actor or a derived type");

 protected:

    typedef sb_actor combined_type;

 public:

    /**
     * @brief Overrides {@link event_based_actor::init()} and sets
     *        the initial actor behavior to <tt>Derived::init_state</tt>.
     */
    virtual void init() {
        this->become(static_cast<Derived*>(this)->init_state);
    }

 protected:

    template<typename... Ts>
    sb_actor(Ts&&... args) : Base(std::forward<Ts>(args)...) { }

};

} // namespace cppa

#endif // CPPA_FSM_ACTOR_HPP
