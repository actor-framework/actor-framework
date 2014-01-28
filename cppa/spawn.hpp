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

#include "cppa/policy.hpp"
#include "cppa/logging.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/typed_actor.hpp"
#include "cppa/spawn_options.hpp"

#include "cppa/util/fiber.hpp"

#include "cppa/detail/proper_actor.hpp"
#include "cppa/detail/functor_based_actor.hpp"
#include "cppa/detail/implicit_conversions.hpp"
#include "cppa/detail/functor_based_blocking_actor.hpp"

namespace cppa {

namespace detail {

template<class Impl, spawn_options Opts, typename BeforeLaunch, typename... Ts>
actor spawn_impl(BeforeLaunch before_launch_fun, Ts&&... args) {
    static_assert(std::is_base_of<event_based_actor, Impl>::value ||
                  (std::is_base_of<blocking_actor, Impl>::value &&
                   has_blocking_api_flag(Opts)),
                  "Impl is not a derived type of event_based_actor or "
                  "is a derived type of blocking_actor but "
                  "blocking_api_flag is missing");
    static_assert(is_unbound(Opts),
                  "top-level spawns cannot have monitor or link flag");
    CPPA_LOGF_TRACE("spawn " << detail::demangle<Impl>());
    // runtime check wheter context_switching_resume can be used,
    // i.e., add the detached flag if libcppa compiled without fiber support
    // when using the blocking API
    if (has_blocking_api_flag(Opts)
            && !has_detach_flag(Opts)
            && util::fiber::is_disabled_feature()) {
        return spawn_impl<Impl, Opts + detached>(before_launch_fun,
                                                    std::forward<Ts>(args)...);
    }
    /*
    using scheduling_policy = typename std::conditional<
                                  has_detach_flag(Opts),
                                  policy::no_scheduling,
                                  policy::cooperative_scheduling
                              >::type;
    */
    using scheduling_policy = policy::no_scheduling;
    using priority_policy = typename std::conditional<
                                has_priority_aware_flag(Opts),
                                policy::prioritizing,
                                policy::not_prioritizing
                            >::type;
    using resume_policy = typename std::conditional<
                              has_blocking_api_flag(Opts),
                              typename std::conditional<
                                  has_detach_flag(Opts),
                                  policy::no_resume,
                                  policy::context_switching_resume
                              >::type,
                              policy::event_based_resume
                          >::type;
    using invoke_policy = typename std::conditional<
                              has_blocking_api_flag(Opts),
                              policy::nestable_invoke,
                              policy::sequential_invoke
                          >::type;
    using policies = policy::policies<scheduling_policy,
                                      priority_policy,
                                      resume_policy,
                                      invoke_policy>;
    using proper_impl = detail::proper_actor<Impl, policies>;
    auto ptr = make_counted<proper_impl>(std::forward<Ts>(args)...);
    CPPA_PUSH_AID(ptr->id());
    before_launch_fun(ptr.get());
    ptr->launch(has_hide_flag(Opts));
    return ptr;
}

template<typename T>
struct spawn_fwd {
    static inline T& fwd(T& arg) { return arg; }
    static inline const T& fwd(const T& arg) { return arg; }
    static inline T&& fwd(T&& arg) { return std::move(arg); }
};

template<typename T, typename... Ts>
struct spawn_fwd<T (Ts...)> {
    typedef T (*pointer) (Ts...);
    static inline pointer fwd(pointer arg) { return arg; }
};

template<>
struct spawn_fwd<scoped_actor> {
    template<typename T>
    static inline actor fwd(T& arg) { return arg; }
};

// forwards the arguments to spawn_impl, replacing pointers
// to actors with instances of 'actor'
template<class Impl, spawn_options Opts, typename BeforeLaunch, typename... Ts>
actor spawn_fwd_args(BeforeLaunch before_launch_fun, Ts&&... args) {
    return spawn_impl<Impl, Opts>(
            before_launch_fun,
            spawn_fwd<typename util::rm_const_and_ref<Ts>::type>::fwd(
                    std::forward<Ts>(args))...);
}

} // namespace detail

/**
 * @ingroup ActorCreation
 * @{
 */

/**
 * @brief Spawns an actor of type @p Impl.
 * @param args Constructor arguments.
 * @tparam Impl Subtype of {@link event_based_actor} or {@link sb_actor}.
 * @tparam Opts Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 */
template<class Impl, spawn_options Opts, typename... Ts>
actor spawn(Ts&&... args) {
    return detail::spawn_fwd_args<Impl, Opts>(
            [](local_actor*) { /* no-op as BeforeLaunch callback */ },
            std::forward<Ts>(args)...);
}

/**
 * @brief Spawns a new {@link actor} that evaluates given arguments.
 * @param args A functor followed by its arguments.
 * @tparam Opts Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 */
//template<spawn_options Opts = no_spawn_options, typename... Ts>
template<spawn_options Opts, typename... Ts>
actor spawn(Ts&&... args) {
    static_assert(sizeof...(Ts) > 0, "too few arguments provided");
    using base_class = typename std::conditional<
                           has_blocking_api_flag(Opts),
                           detail::functor_based_blocking_actor,
                           detail::functor_based_actor
                       >::type;
    return spawn<base_class, Opts>(std::forward<Ts>(args)...);
}

/**
 * @brief Spawns a new actor that evaluates given arguments and
 *        immediately joins @p grp.
 * @param args A functor followed by its arguments.
 * @tparam Opts Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 * @note The spawned has joined the group before this function returns.
 */
template<spawn_options Opts, typename... Ts>
actor spawn_in_group(const group& grp, Ts&&... args) {
    static_assert(sizeof...(Ts) > 0, "too few arguments provided");
    using base_class = typename std::conditional<
                           has_blocking_api_flag(Opts),
                           detail::functor_based_blocking_actor,
                           detail::functor_based_actor
                       >::type;
    return detail::spawn_fwd_args<base_class, Opts>(
            [&](local_actor* ptr) { ptr->join(grp); },
            std::forward<Ts>(args)...);
}

/**
 * @brief Spawns an actor of type @p Impl that immediately joins @p grp.
 * @param args Constructor arguments.
 * @tparam Impl Subtype of {@link event_based_actor} or {@link sb_actor}.
 * @tparam Opts Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 * @note The spawned has joined the group before this function returns.
 */
template<class Impl, spawn_options Opts, typename... Ts>
actor spawn_in_group(const group& grp, Ts&&... args) {
    return detail::spawn_fwd_args<Impl, Opts>(
            [&](local_actor* ptr) { ptr->join(grp); },
            std::forward<Ts>(args)...);
}

/*
template<class Impl, spawn_options Opts = no_spawn_options, typename... Ts>
typename Impl::typed_pointer_type spawn_typed(Ts&&... args) {
    static_assert(util::tl_is_distinct<typename Impl::signatures>::value,
                  "typed actors are not allowed to define "
                  "multiple patterns with identical signature");
    auto p = make_counted<Impl>(std::forward<Ts>(args)...);
    using result_type = typename Impl::typed_pointer_type;
    return result_type::cast_from(
        eval_sopts(Opts, get_scheduler()->exec(Opts, std::move(p)))
    );
}
*/

/*TODO:
template<spawn_options Opts, typename... Ts>
typed_actor<typename detail::deduce_signature<Ts>::type...>
spawn_typed(const match_expr<Ts...>& me) {
    static_assert(util::conjunction<
                      detail::match_expr_has_no_guard<Ts>::value...
                  >::value,
                  "typed actors are not allowed to use guard expressions");
    static_assert(util::tl_is_distinct<
                      util::type_list<
                          typename detail::deduce_signature<Ts>::arg_types...
                      >
                  >::value,
                  "typed actors are not allowed to define "
                  "multiple patterns with identical signature");
    using impl = detail::default_typed_actor<
                     typename detail::deduce_signature<Ts>::type...
                 >;
    return spawn_typed<impl, Opts>(me);
}

template<typename... Ts>
typed_actor<typename detail::deduce_signature<Ts>::type...>
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
*/

/** @} */

} // namespace cppa

#endif // CPPA_SPAWN_HPP
