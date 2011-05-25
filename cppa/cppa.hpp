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

#ifndef CPPA_HPP
#define CPPA_HPP

#include <tuple>
#include <type_traits>

#include "cppa/tuple.hpp"
#include "cppa/actor.hpp"
#include "cppa/invoke.hpp"
#include "cppa/channel.hpp"
#include "cppa/context.hpp"
#include "cppa/message.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/scheduling_hint.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"

#include "cppa/detail/tdata.hpp"

namespace cppa {

namespace detail {

template<bool IsFunctionPtr, typename F>
class fun_behavior : public actor_behavior
{

    F m_fun;

 public:

    fun_behavior(F ptr) : m_fun(ptr) { }

    virtual void act()
    {
        m_fun();
    }

};

template<typename F>
class fun_behavior<false, F> : public actor_behavior
{

    F m_fun;

 public:

    fun_behavior(const F& arg) : m_fun(arg) { }

    fun_behavior(F&& arg) : m_fun(std::move(arg)) { }

    virtual void act()
    {
        m_fun();
    }

};

template<typename R>
actor_behavior* get_behavior(std::integral_constant<bool,true>, R (*fptr)())
{
    return new fun_behavior<true, R (*)()>(fptr);
}

template<typename F>
actor_behavior* get_behavior(std::integral_constant<bool,false>, F&& ftor)
{
    typedef typename util::rm_ref<F>::type ftype;
    return new fun_behavior<false, ftype>(std::forward<F>(ftor));
}

template<typename F, typename Arg0, typename... Args>
actor_behavior* get_behavior(std::integral_constant<bool,true>,
                             F fptr,
                             const Arg0& arg0,
                             const Args&... args)
{
    detail::tdata<Arg0, Args...> arg_tuple(arg0, args...);
    auto lambda = [fptr, arg_tuple]() { invoke(fptr, arg_tuple); };
    return new fun_behavior<false, decltype(lambda)>(std::move(lambda));
}

template<typename F, typename Arg0, typename... Args>
actor_behavior* get_behavior(std::integral_constant<bool,false>,
                             F ftor,
                             const Arg0& arg0,
                             const Args&... args)
{
    detail::tdata<Arg0, Args...> arg_tuple(arg0, args...);
    auto lambda = [ftor, arg_tuple]() { invoke(ftor, arg_tuple); };
    return new fun_behavior<false, decltype(lambda)>(std::move(lambda));
}

} // namespace detail

template<scheduling_hint Hint, typename F, typename... Args>
actor_ptr spawn(F&& what, const Args&... args)
{
    typedef typename util::rm_ref<F>::type ftype;
    std::integral_constant<bool, std::is_function<ftype>::value> is_fun;
    auto ptr = detail::get_behavior(is_fun, std::forward<F>(what), args...);
    return get_scheduler()->spawn(ptr, Hint);
}

template<typename F, typename... Args>
actor_ptr spawn(F&& what, const Args&... args)
{
    return spawn<scheduled>(std::forward<F>(what), args...);
}

/*
template<typename F>
actor_ptr spawn(scheduling_hint hint, const F& fun)
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
    return spawn(hint, new fun_behavior(fun));
}

template<typename F, typename Arg0, typename... Args>
actor_ptr spawn(scheduling_hint hint, const F& fun, const Arg0& arg0, const Args&... args)
{
    auto arg_tuple = make_tuple(arg0, args...);
    return spawn(hint, [=]() { invoke(fun, arg_tuple); });
}

template<typename F, typename... Args>
inline actor_ptr spawn(const F& fun, const Args&... args)
{
    return spawn(scheduled, fun, args...);
}
*/

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

/**
 * @brief Blocks execution of this actor until all
 *        other actors finished execution.
 * @warning This function will cause a deadlock if
 *          called from multiple actors.
 */
inline void await_all_others_done()
{
    get_scheduler()->await_others_done();
}

} // namespace cppa

#endif // CPPA_HPP
