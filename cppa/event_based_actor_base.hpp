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

/**
 * @brief Base class for event-based actor implementations.
 */
template<typename Derived>
class event_based_actor_base : public abstract_event_based_actor
{

    typedef abstract_event_based_actor super;

    inline Derived* d_this() { return static_cast<Derived*>(this); }

 protected:

    /**
     * @brief Sets the actor's behavior to @p bhvr.
     * @note @p bhvr is owned by caller and must remain valid until
     *       the actor terminates.
     *       This member function should be used to use a member of
     *       a subclass as behavior.
     */
    inline void become(behavior* bhvr)
    {
        d_this()->do_become(bhvr, false);
    }

    /** @brief Sets the actor's behavior. */
    template<typename... Args>
    void become(partial_function&& arg0, Args&&... args)
    {
        auto ptr = new behavior;
        ptr->splice(std::move(arg0), std::forward<Args>(args)...);
        d_this()->do_become(ptr, true);
    }

    /** @brief Sets the actor's behavior to @p bhvr. */
    inline void become(behavior&& bhvr)
    {
        d_this()->do_become(new behavior(std::move(bhvr)), true);
    }

};

} // namespace cppa

#endif // EVENT_BASED_ACTOR_MIXIN_HPP
