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
 * Copyright (C) 2011, Dominik Charousset <dominik.charousset@haw-hamburg.de> *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as  published by the    *
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

#ifndef CPPA_HPP
#define CPPA_HPP

#include "cppa/tuple.hpp"
#include "cppa/actor.hpp"
#include "cppa/invoke.hpp"
#include "cppa/channel.hpp"
#include "cppa/context.hpp"
#include "cppa/message.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/scheduling_hint.hpp"
#include "cppa/scheduler.hpp"

namespace cppa {

template<typename F>
actor_ptr spawn(scheduling_hint hint, F fun)
{
    struct fun_behavior : actor_behavior
    {
        F m_fun;
        fun_behavior(const F& fun_arg) : m_fun(fun_arg) { }
        virtual void act()
        {
            m_fun();
        }
    };
    return get_scheduler()->spawn(new fun_behavior(fun), hint);
}

template<typename F, typename Arg0, typename... Args>
actor_ptr spawn(scheduling_hint hint, F fun, const Arg0& arg0, const Args&... args)
{
    auto arg_tuple = make_tuple(arg0, args...);
    return spawn(hint, [=]() { invoke(fun, arg_tuple); });
}

template<typename F, typename... Args>
inline actor_ptr spawn(F fun, const Args&... args)
{
    return spawn(scheduled, fun, args...);
}

inline actor_ptr spawn(scheduling_hint hint, actor_behavior* ab)
{
    return get_scheduler()->spawn(ab, hint);
}

inline actor_ptr spawn(actor_behavior* ab)
{
    return get_scheduler()->spawn(ab, scheduled);
}

inline const message& receive()
{
    return self()->mailbox().dequeue();
}

inline void receive(invoke_rules& rules)
{
    self()->mailbox().dequeue(rules);
}

inline void receive(invoke_rules&& rules)
{
    self()->mailbox().dequeue(rules);
}

inline bool try_receive(message& msg)
{
    return self()->mailbox().try_dequeue(msg);
}

inline bool try_receive(invoke_rules& rules)
{
    return self()->mailbox().try_dequeue(rules);
}

inline const message& last_received()
{
    return self()->mailbox().last_dequeued();
}

template<typename Arg0, typename... Args>
void send(channel_ptr whom, const Arg0& arg0, const Args&... args)
{
    if (whom) whom->enqueue(message(self(), whom, arg0, args...));
}

template<typename Arg0, typename... Args>
void reply(const Arg0& arg0, const Args&... args)
{
    context* sptr = self();
    actor_ptr whom = sptr->mailbox().last_dequeued().sender();
    if (whom) whom->enqueue(message(sptr, whom, arg0, args...));
}

inline void await_all_others_done()
{
    get_scheduler()->await_others_done();
}

} // namespace cppa

#endif // CPPA_HPP
