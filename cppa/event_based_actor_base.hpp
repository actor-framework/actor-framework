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


#ifndef EVENT_BASED_ACTOR_MIXIN_HPP
#define EVENT_BASED_ACTOR_MIXIN_HPP

#include "cppa/behavior.hpp"
#include "cppa/abstract_event_based_actor.hpp"

namespace cppa {

template<typename Derived>
class event_based_actor_base : public abstract_event_based_actor
{

    typedef abstract_event_based_actor super;

    inline void become_impl(invoke_rules& rules)
    {
        become(std::move(rules));
    }

    inline void become_impl(timed_invoke_rules&& rules)
    {
        become(std::move(rules));
    }

    template<typename Head, typename... Tail>
    void become_impl(invoke_rules& rules, Head&& head, Tail&&... tail)
    {
        become_impl(rules.splice(std::forward<Head>(head)),
                    std::forward<Tail>(tail)...);
    }

    inline Derived* d_this() { return static_cast<Derived*>(this); }

 protected:

    inline void become(behavior* bhvr)
    {
        d_this()->do_become(bhvr);
    }

    inline void become(invoke_rules* bhvr)
    {
        d_this()->do_become(bhvr, false);
    }

    inline void become(timed_invoke_rules* bhvr)
    {
        d_this()->do_become(bhvr, false);
    }

    inline void become(invoke_rules&& bhvr)
    {
        d_this()->do_become(new invoke_rules(std::move(bhvr)), true);
    }

    inline void become(timed_invoke_rules&& bhvr)
    {
        d_this()->do_become(new timed_invoke_rules(std::move(bhvr)), true);
    }

    inline void become(behavior&& bhvr)
    {
        if (bhvr.is_left()) become(std::move(bhvr.left()));
        else become(std::move(bhvr.right()));
    }

    template<typename Head, typename... Tail>
    void become(invoke_rules&& rules, Head&& head, Tail&&... tail)
    {
        invoke_rules tmp(std::move(rules));
        become_impl(tmp.splice(std::forward<Head>(head)),
                    std::forward<Tail>(tail)...);
    }

};

} // namespace cppa

#endif // EVENT_BASED_ACTOR_MIXIN_HPP
