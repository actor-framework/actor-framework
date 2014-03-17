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


#ifndef CPPA_FUNCTOR_BASED_ACTOR_HPP
#define CPPA_FUNCTOR_BASED_ACTOR_HPP

#include <type_traits>

#include "cppa/event_based_actor.hpp"

namespace cppa { namespace detail {

class functor_based_actor : public event_based_actor {

 public:

    typedef event_based_actor* pointer;

    typedef std::function<behavior(event_based_actor*)> make_behavior_fun;

    typedef std::function<void(event_based_actor*)> void_fun;

    template<typename F, typename... Ts>
    functor_based_actor(F f, Ts&&... vs) : m_void_impl(false) {
        typedef typename util::get_callable_trait<F>::type trait;
        typedef typename trait::arg_types arg_types;
        typedef typename trait::result_type result_type;
        constexpr bool returns_behavior = std::is_convertible<
                                              result_type,
                                              behavior
                                          >::value;
        constexpr bool uses_first_arg = std::is_same<
                                            typename util::tl_head<
                                                arg_types
                                            >::type,
                                            pointer
                                        >::value;
        std::integral_constant<bool, returns_behavior> token1;
        std::integral_constant<bool, uses_first_arg>   token2;
        set(token1, token2, std::move(f), std::forward<Ts>(vs)...);
    }

    behavior make_behavior() override;

 private:

    template<typename F>
    void set(std::true_type, std::true_type, F&& fun) {
        // behavior (pointer)
        m_make_behavior = std::forward<F>(fun);
    }

    template<typename F>
    void set(std::false_type, std::true_type, F fun) {
        // void (pointer)
        m_void_impl = true;
        m_make_behavior = [fun](pointer ptr) {
            fun(ptr);
            return behavior{};
        };
    }

    template<typename F>
    void set(std::true_type, std::false_type, F fun) {
        // behavior (void)
        m_make_behavior = [fun](pointer) { return fun(); };
    }

    template<typename F>
    void set(std::false_type, std::false_type, F fun) {
        // void (void)
        m_void_impl = true;
        m_make_behavior = [fun](pointer) {
            fun();
            return behavior{};
        };
    }

    template<class Token, typename F, typename T0, typename... Ts>
    void set(Token t1, std::true_type t2, F fun, T0&& arg0, Ts&&... args) {
        set(t1, t2, std::bind(fun,
                              std::placeholders::_1,
                              std::forward<T0>(arg0),
                              std::forward<Ts>(args)...));
    }

    template<class Token, typename F, typename T0, typename... Ts>
    void set(Token t1, std::false_type t2, F fun, T0&& arg0,  Ts&&... args) {
        set(t1, t2, std::bind(fun,
                              std::forward<T0>(arg0),
                              std::forward<Ts>(args)...));
    }

    bool m_void_impl;
    make_behavior_fun m_make_behavior;

};

} } // namespace cppa::detail

#endif // CPPA_FUNCTOR_BASED_ACTOR_HPP
