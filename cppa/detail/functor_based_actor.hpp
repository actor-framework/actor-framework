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

#include "cppa/untyped_actor.hpp"

namespace cppa { namespace detail {

class functor_based_actor : public untyped_actor {

 public:

    typedef std::function<behavior(untyped_actor*)> make_behavior_fun;

    typedef std::function<void(untyped_actor*)> void_fun;

    template<typename F, typename... Ts>
    functor_based_actor(F f, Ts&&... vs) {
        untyped_actor* dummy = nullptr;
        create(dummy, f, std::forward<Ts>(vs)...);
    }

    behavior make_behavior() override;

 private:

    void create(untyped_actor*, void_fun);

    template<class Actor, typename F, typename... Ts>
    auto create(Actor*, F f, Ts&&... vs) ->
    typename std::enable_if<
        std::is_convertible<
            decltype(f(std::forward<Ts>(vs)...)),
            behavior
        >::value
    >::type {
        auto fun = std::bind(f, std::forward<Ts>(vs)...);
        m_make_behavior = [fun](Actor*) -> behavior { return fun(); };
    }

    template<class Actor, typename F, typename... Ts>
    auto create(Actor* dummy, F f, Ts&&... vs) ->
    typename std::enable_if<
        std::is_convertible<
            decltype(f(dummy, std::forward<Ts>(vs)...)),
            behavior
        >::value
    >::type {
        auto fun = std::bind(f, std::placeholders::_1, std::forward<Ts>(vs)...);
        m_make_behavior = [fun](Actor* self) -> behavior { return fun(self); };
    }

    template<class Actor, typename F, typename... Ts>
    auto create(Actor*, F f, Ts&&... vs) ->
    typename std::enable_if<
        std::is_same<
            decltype(f(std::forward<Ts>(vs)...)),
            void
        >::value
    >::type {
        std::function<void()> fun = std::bind(f, std::forward<Ts>(vs)...);
        m_make_behavior = [fun](Actor*) -> behavior {
            fun();
            return behavior{};
        };
    }

    template<class Actor, typename F, typename... Ts>
    auto create(Actor* dummy, F f, Ts&&... vs) ->
    typename std::enable_if<
        std::is_same<
            decltype(f(dummy, std::forward<Ts>(vs)...)),
            void
        >::value
    >::type {
        std::function<void(Actor*)> fun = std::bind(f, std::placeholders::_1,
                                                    std::forward<Ts>(vs)...);
        m_make_behavior = [fun](Actor* self) -> behavior {
            fun(self);
            return behavior{};
        };
    }

    make_behavior_fun m_make_behavior;

};

} } // namespace cppa::detail

#endif // CPPA_FUNCTOR_BASED_ACTOR_HPP
