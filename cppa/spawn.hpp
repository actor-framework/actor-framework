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


#ifndef CPPA_SPAWN_HPP
#define CPPA_SPAWN_HPP

#include <type_traits>

#include "cppa/threaded.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/typed_actor.hpp"
#include "cppa/prioritizing.hpp"
#include "cppa/spawn_options.hpp"
#include "cppa/event_based_actor.hpp"

namespace cppa {

/** @cond PRIVATE */

inline actor_ptr eval_sopts(spawn_options opts, local_actor_ptr ptr) {
    CPPA_LOGF_INFO("spawned new local actor with ID " << ptr->id()
                   << " of type " << detail::demangle(typeid(*ptr)));
    if (has_monitor_flag(opts)) self->monitor(ptr);
    if (has_link_flag(opts)) self->link_to(ptr);
    return std::move(ptr);
}

/** @endcond */

/**
 * @ingroup ActorCreation
 * @{
 */

/**
 * @brief Spawns a new {@link actor} that evaluates given arguments.
 * @param args A functor followed by its arguments.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 */
template<spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn(Ts&&... args) {
    static_assert(sizeof...(Ts) > 0, "too few arguments provided");
    return eval_sopts(Options,
                      get_scheduler()->exec(Options,
                                            scheduler::init_callback{},
                                            std::forward<Ts>(args)...));
}

/**
 * @brief Spawns an actor of type @p Impl.
 * @param args Constructor arguments.
 * @tparam Impl Subtype of {@link event_based_actor} or {@link sb_actor}.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 */
template<class Impl, spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn(Ts&&... args) {
    static_assert(std::is_base_of<event_based_actor, Impl>::value,
                  "Impl is not a derived type of event_based_actor");
    scheduled_actor_ptr ptr;
    if (has_priority_aware_flag(Options)) {
        using derived = typename extend<Impl>::template with<threaded, prioritizing>;
        ptr = make_counted<derived>(std::forward<Ts>(args)...);
    }
    else if (has_detach_flag(Options)) {
        using derived = typename extend<Impl>::template with<threaded>;
        ptr = make_counted<derived>(std::forward<Ts>(args)...);
    }
    else ptr = make_counted<Impl>(std::forward<Ts>(args)...);
    return eval_sopts(Options, get_scheduler()->exec(Options, std::move(ptr)));
}

/**
 * @brief Spawns a new actor that evaluates given arguments and
 *        immediately joins @p grp.
 * @param args A functor followed by its arguments.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 * @note The spawned has joined the group before this function returns.
 */
template<spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn_in_group(const group_ptr& grp, Ts&&... args) {
    static_assert(sizeof...(Ts) > 0, "too few arguments provided");
    auto init_cb = [=](local_actor* ptr) {
        ptr->join(grp);
    };
    return eval_sopts(Options,
                      get_scheduler()->exec(Options,
                                            init_cb,
                                            std::forward<Ts>(args)...));
}

/**
 * @brief Spawns an actor of type @p Impl that immediately joins @p grp.
 * @param args Constructor arguments.
 * @tparam Impl Subtype of {@link event_based_actor} or {@link sb_actor}.
 * @tparam Options Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor_ptr} to the spawned {@link actor}.
 * @note The spawned has joined the group before this function returns.
 */
template<class Impl, spawn_options Options = no_spawn_options, typename... Ts>
actor_ptr spawn_in_group(const group_ptr& grp, Ts&&... args) {
    auto ptr = make_counted<Impl>(std::forward<Ts>(args)...);
    ptr->join(grp);
    return eval_sopts(Options, get_scheduler()->exec(Options, ptr));
}

template<spawn_options Options, typename... Ts>
typed_actor_ptr<typename detail::deduce_signature<Ts>::type...>
spawn_typed(const match_expr<Ts...>& me) {

    static_assert(util::conjunction<
                      detail::match_expr_has_no_guard<Ts>::value...
                  >::value,
                  "typed actors are not allowed to use guard expressions");

    typedef util::type_list<
                typename detail::deduce_signature<Ts>::arg_types...
            >
            args;

    static_assert(util::tl_is_distinct<args>::value,
                  "typed actors are not allowed to define multiple patterns "
                  "with identical signature");

    auto ptr = make_counted<typed_actor<match_expr<Ts...>>>(me);

    return {eval_sopts(Options, get_scheduler()->exec(Options, std::move(ptr)))};

}

template<typename... Ts>
typed_actor_ptr<typename detail::deduce_signature<Ts>::type...>
spawn_typed(const match_expr<Ts...>& me) {
    return spawn_typed<no_spawn_options>(me);
}

template<typename T0, typename T1, typename... Ts>
auto spawn_typed(T0&& v0, T1&& v1, Ts&&... vs)
-> decltype(spawn_typed(match_expr_collect(std::forward<T0>(v0),
                                           std::forward<T1>(v1),
                                           std::forward<Ts>(vs)...))) {
    return spawn_typed(match_expr_collect(std::forward<T0>(v0),
                                          std::forward<T1>(v1),
                                          std::forward<Ts>(vs)...));
}

/** @} */

} // namespace cppa

#endif // CPPA_SPAWN_HPP
