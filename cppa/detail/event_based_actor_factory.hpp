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


#ifndef CPPA_EVENT_BASED_ACTOR_FACTORY_HPP
#define CPPA_EVENT_BASED_ACTOR_FACTORY_HPP

#include <type_traits>

#include "cppa/scheduler.hpp"
#include "cppa/event_based_actor.hpp"

#include "cppa/detail/tdata.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa { namespace detail {

template<typename InitFun, typename CleanupFun, typename... Members>
class event_based_actor_impl : public event_based_actor {

 public:

    template<typename... Args>
    event_based_actor_impl(InitFun fun, CleanupFun cfun, Args&&... args)
    : m_init(std::move(fun)), m_on_exit(std::move(cfun))
    , m_members(std::forward<Args>(args)...) { }

    void init() { apply(m_init); }

    void on_exit() {
        typedef typename util::get_arg_types<CleanupFun>::types arg_types;
        std::integral_constant<size_t, arg_types::size> token;
        on_exit_impl(m_on_exit, token);
    }

 private:

    InitFun m_init;
    CleanupFun m_on_exit;
    tdata<Members...> m_members;

    template<typename F>
    void apply(F& f, typename std::add_pointer<Members>::type... args) {
        f(args...);
    }

    template<typename F, typename... Args>
    void apply(F& f, Args... args) {
        apply(f, args..., &get_ref<sizeof...(Args)>(m_members));
    }

    typedef std::integral_constant<size_t, 0> zero_t;

    template<typename OnExit, typename Token>
    typename std::enable_if<std::is_same<Token, zero_t>::value>::type
    on_exit_impl(OnExit& fun, Token) {
        fun();
    }

    template<typename OnExit, typename Token>
    typename std::enable_if<std::is_same<Token, zero_t>::value == false>::type
    on_exit_impl(OnExit& fun, Token) {
        apply(fun);
    }

};

template<typename InitFun, typename CleanupFun, typename... Members>
class event_based_actor_factory {

 public:

    typedef event_based_actor_impl<InitFun, CleanupFun, Members...> impl;

    event_based_actor_factory(InitFun fun, CleanupFun cfun)
    : m_init(std::move(fun)), m_on_exit(std::move(cfun)) { }

    template<typename... Args>
    actor_ptr spawn(Args&&... args) {
        return get_scheduler()->spawn(new impl(m_init, m_on_exit,
                                               std::forward<Args>(args)...));
    }

 private:

    InitFun m_init;
    CleanupFun m_on_exit;

};

// event-based actor factory from type list
template<typename InitFun, typename CleanupFun, class TypeList>
struct ebaf_from_type_list;

template<typename InitFun, typename CleanupFun, typename... Ts>
struct ebaf_from_type_list<InitFun, CleanupFun, util::type_list<Ts...> > {
    typedef event_based_actor_factory<InitFun, CleanupFun, Ts...> type;
};

template<typename Init, typename Cleanup>
struct ebaf_from_functor {
    typedef typename util::get_arg_types<Init>::types arg_types;
    typedef typename util::get_arg_types<Cleanup>::types arg_types2;
    static_assert(util::tl_forall<arg_types, std::is_pointer>::value,
                  "First functor takes non-pointer arguments");
    static_assert(   std::is_same<arg_types, arg_types2>::value
                  || std::is_same<util::type_list<>, arg_types2>::value,
                  "Second functor must provide either the same signature "
                  " as the first one or must take zero arguments");
    typedef typename util::tl_map<arg_types, std::remove_pointer>::type mems;
    typedef typename ebaf_from_type_list<Init, Cleanup, mems>::type type;
};

} } // namespace cppa::detail

#endif // CPPA_EVENT_BASED_ACTOR_FACTORY_HPP
