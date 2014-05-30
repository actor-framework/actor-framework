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


#ifndef CPPA_DETAIL_FUNCTOR_BASED_BLOCKING_ACTOR_HPP
#define CPPA_DETAIL_FUNCTOR_BASED_BLOCKING_ACTOR_HPP

#include "cppa/blocking_actor.hpp"

namespace cppa {
namespace detail {

class functor_based_blocking_actor : public blocking_actor {

 public:

    typedef std::function<void (blocking_actor*)> act_fun;

    template<typename F, typename... Ts>
    functor_based_blocking_actor(F f, Ts&&... vs) {
        blocking_actor* dummy = nullptr;
        create(dummy, f, std::forward<Ts>(vs)...);
    }

 protected:

    void act() override;

 private:

    void create(blocking_actor*, act_fun);

    template<class Actor, typename F, typename T0, typename... Ts>
    auto create(Actor* dummy, F f, T0&& v0, Ts&&... vs) ->
    typename std::enable_if<
        std::is_same<
            decltype(f(dummy, std::forward<T0>(v0), std::forward<Ts>(vs)...)),
            void
        >::value
    >::type {
        create(dummy, std::bind(f, std::placeholders::_1,
                                std::forward<T0>(v0), std::forward<Ts>(vs)...));
    }

    template<class Actor, typename F, typename T0, typename... Ts>
    auto create(Actor* dummy, F f, T0&& v0, Ts&&... vs) ->
    typename std::enable_if<
        std::is_same<
            decltype(f(std::forward<T0>(v0), std::forward<Ts>(vs)...)),
            void
        >::value
    >::type {
        std::function<void()> fun = std::bind(f, std::forward<T0>(v0),
                                              std::forward<Ts>(vs)...);
        create(dummy, [fun](Actor*) { fun(); });
    }

    act_fun m_act;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_FUNCTOR_BASED_BLOCKING_ACTOR_HPP
