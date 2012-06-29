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


#ifndef CPPA_GET_BEHAVIOR_HPP
#define CPPA_GET_BEHAVIOR_HPP

#include <type_traits>

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/apply_tuple.hpp"

#include "cppa/detail/tdata.hpp"
#include "cppa/scheduled_actor.hpp"

namespace cppa { namespace detail {

// default: <true, false, F>
template<bool IsFunctionPtr, bool HasArguments, typename F, typename... Args>
class ftor_behavior : public scheduled_actor {

    F m_fun;

 public:

    ftor_behavior(F ptr) : m_fun(ptr) { }

    virtual void act() { m_fun(); }

};

template<typename F, typename... Args>
class ftor_behavior<true, true, F, Args...>  : public scheduled_actor {

    static_assert(sizeof...(Args) > 0, "sizeof...(Args) == 0");

    F m_fun;

    typedef typename tdata_from_type_list<
        typename util::tl_map<util::type_list<Args...>,
                                implicit_conversions>::type>::type
        tdata_type;

    tdata_type m_args;

 public:

    ftor_behavior(F ptr, const Args&... args) : m_fun(ptr), m_args(args...) { }

    virtual void act() { util::apply_tuple(m_fun, m_args); }

};

template<typename F>
class ftor_behavior<false, false, F> : public scheduled_actor {

    F m_fun;

 public:

    ftor_behavior(const F& arg) : m_fun(arg) { }

    ftor_behavior(F&& arg) : m_fun(std::move(arg)) { }

    virtual void act() { m_fun(); }

};

template<typename F, typename... Args>
class ftor_behavior<false, true, F, Args...>  : public scheduled_actor {

    static_assert(sizeof...(Args) > 0, "sizeof...(Args) == 0");

    F m_fun;

    typedef typename tdata_from_type_list<
        typename util::tl_map<util::type_list<Args...>,
                                implicit_conversions>::type>::type
        tdata_type;

    tdata_type m_args;

 public:

    ftor_behavior(const F& f, const Args&... args) : m_fun(f), m_args(args...) {
    }

    ftor_behavior(F&& f,const Args&... args) : m_fun(std::move(f))
                                             , m_args(args...) {
    }

    virtual void act() { util::apply_tuple(m_fun, m_args); }

};

template<typename R>
scheduled_actor* get_behavior(std::integral_constant<bool,true>, R (*fptr)()) {
    static_assert(std::is_convertible<R, scheduled_actor*>::value == false,
                  "Spawning a function returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that function?");
    return new ftor_behavior<true, false, R (*)()>(fptr);
}

template<typename F>
scheduled_actor* get_behavior(std::integral_constant<bool,false>, F&& ftor) {
    static_assert(std::is_convertible<decltype(ftor()), scheduled_actor*>::value == false,
                  "Spawning a functor returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that functor?");
    typedef typename util::rm_ref<F>::type ftype;
    return new ftor_behavior<false, false, ftype>(std::forward<F>(ftor));
}

template<typename F, typename Arg0, typename... Args>
scheduled_actor* get_behavior(std::integral_constant<bool,true>,
                             F fptr,
                             const Arg0& arg0,
                             const Args&... args) {
    static_assert(std::is_convertible<decltype(fptr(arg0, args...)), scheduled_actor*>::value == false,
                  "Spawning a function returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that function?");
    typedef ftor_behavior<true, true, F, Arg0, Args...> impl;
    return new impl(fptr, arg0, args...);
}

template<typename F, typename Arg0, typename... Args>
scheduled_actor* get_behavior(std::integral_constant<bool,false>,
                             F ftor,
                             const Arg0& arg0,
                             const Args&... args) {
    static_assert(std::is_convertible<decltype(ftor(arg0, args...)), scheduled_actor*>::value == false,
                  "Spawning a functor returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that functor?");
    typedef typename util::rm_ref<F>::type ftype;
    typedef ftor_behavior<false, true, ftype, Arg0, Args...> impl;
    return new impl(std::forward<F>(ftor), arg0, args...);
}

} } // namespace cppa::detail

#endif // CPPA_GET_BEHAVIOR_HPP
