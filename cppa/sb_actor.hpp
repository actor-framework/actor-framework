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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
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

    static_assert(std::is_base_of<event_based_actor, Base>::value,
                  "Base must be event_based_actor or a derived type");

 protected:

    using combined_type = sb_actor;

 public:

    /**
     * @brief Overrides {@link event_based_actor::make_behavior()} and sets
     *        the initial actor behavior to <tt>Derived::init_state</tt>.
     */
    behavior make_behavior() override {
        return static_cast<Derived*>(this)->init_state;
    }

 protected:

    template<typename... Ts>
    sb_actor(Ts&&... args)
            : Base(std::forward<Ts>(args)...) {}

};

} // namespace cppa

#endif // CPPA_FSM_ACTOR_HPP
