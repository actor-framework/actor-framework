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
#include "cppa/spawn_fwd.hpp"
#include "cppa/typed_actor.hpp"
#include "cppa/spawn_options.hpp"
#include "cppa/typed_event_based_actor.hpp"

#include "cppa/detail/cs_thread.hpp"
#include "cppa/util/type_traits.hpp"

#include "cppa/detail/proper_actor.hpp"
#include "cppa/detail/typed_actor_util.hpp"
#include "cppa/detail/functor_based_actor.hpp"
#include "cppa/detail/implicit_conversions.hpp"
#include "cppa/detail/functor_based_blocking_actor.hpp"

namespace cppa {

namespace detail {

template<class C, spawn_options Os, typename BeforeLaunch, typename... Ts>
intrusive_ptr<C> spawn_impl(BeforeLaunch before_launch_fun, Ts&&... args) {
    static_assert(!std::is_base_of<blocking_actor, C>::value ||
                  has_blocking_api_flag(Os),
                  "C is derived type of blocking_actor but "
                  "blocking_api_flag is missing");
    static_assert(is_unbound(Os),
                  "top-level spawns cannot have monitor or link flag");
    CPPA_LOGF_TRACE("spawn " << detail::demangle<C>());
    // runtime check wheter context_switching_resume can be used,
    // i.e., add the detached flag if libcppa was compiled
    // without cs_thread support when using the blocking API
    if (has_blocking_api_flag(Os)
            && !has_detach_flag(Os)
            && detail::cs_thread::is_disabled_feature) {
        return spawn_impl<C, Os + detached>(before_launch_fun,
                                            std::forward<Ts>(args)...);
    }
    /*
    using scheduling_policy = typename std::conditional<
                                  has_detach_flag(Os),
                                  policy::no_scheduling,
                                  policy::cooperative_scheduling
                              >::type;
    */
    using scheduling_policy = policy::no_scheduling;
    using priority_policy = typename std::conditional<
                                has_priority_aware_flag(Os),
                                policy::prioritizing,
                                policy::not_prioritizing
                            >::type;
    using resume_policy = typename std::conditional<
                              has_blocking_api_flag(Os),
                              typename std::conditional<
                                  has_detach_flag(Os),
                                  policy::no_resume,
                                  policy::context_switching_resume
                              >::type,
                              policy::event_based_resume
                          >::type;
    using invoke_policy = typename std::conditional<
                              has_blocking_api_flag(Os),
                              policy::nestable_invoke,
                              policy::sequential_invoke
                          >::type;
    using policies = policy::policies<scheduling_policy,
                                      priority_policy,
                                      resume_policy,
                                      invoke_policy>;
    using proper_impl = detail::proper_actor<C, policies>;
    auto ptr = make_counted<proper_impl>(std::forward<Ts>(args)...);
    CPPA_PUSH_AID(ptr->id());
    before_launch_fun(ptr.get());
    ptr->launch(has_hide_flag(Os));
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
template<class C, spawn_options Os, typename BeforeLaunch, typename... Ts>
intrusive_ptr<C> spawn_fwd_args(BeforeLaunch before_launch_fun, Ts&&... args) {
    return spawn_impl<C, Os>(
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
 * @brief Spawns an actor of type @p C.
 * @param args Constructor arguments.
 * @tparam C Subtype of {@link event_based_actor} or {@link sb_actor}.
 * @tparam Os Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 */
template<class C, spawn_options Os, typename... Ts>
actor spawn(Ts&&... args) {
    return detail::spawn_fwd_args<C, Os>(
            [](C*) { /* no-op as BeforeLaunch callback */ },
            std::forward<Ts>(args)...);
}

/**
 * @brief Spawns a new {@link actor} that evaluates given arguments.
 * @param args A functor followed by its arguments.
 * @tparam Os Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 */
//template<spawn_options Os = no_spawn_options, typename... Ts>
template<spawn_options Os, typename... Ts>
actor spawn(Ts&&... args) {
    static_assert(sizeof...(Ts) > 0, "too few arguments provided");
    using base_class = typename std::conditional<
                           has_blocking_api_flag(Os),
                           detail::functor_based_blocking_actor,
                           detail::functor_based_actor
                       >::type;
    return spawn<base_class, Os>(std::forward<Ts>(args)...);
}

/**
 * @brief Spawns a new actor that evaluates given arguments and
 *        immediately joins @p grp.
 * @param args A functor followed by its arguments.
 * @tparam Os Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 * @note The spawned has joined the group before this function returns.
 */
template<spawn_options Os, typename... Ts>
actor spawn_in_group(const group& grp, Ts&&... args) {
    static_assert(sizeof...(Ts) > 0, "too few arguments provided");
    using base_class = typename std::conditional<
                           has_blocking_api_flag(Os),
                           detail::functor_based_blocking_actor,
                           detail::functor_based_actor
                       >::type;
    return detail::spawn_fwd_args<base_class, Os>(
            [&](base_class* ptr) { ptr->join(grp); },
            std::forward<Ts>(args)...);
}

/**
 * @brief Spawns an actor of type @p C that immediately joins @p grp.
 * @param args Constructor arguments.
 * @tparam C Subtype of {@link event_based_actor} or {@link sb_actor}.
 * @tparam Os Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 * @note The spawned has joined the group before this function returns.
 */
template<class C, spawn_options Os, typename... Ts>
actor spawn_in_group(const group& grp, Ts&&... args) {
    return detail::spawn_fwd_args<C, Os>(
            [&](C* ptr) { ptr->join(grp); },
            std::forward<Ts>(args)...);
}

namespace detail {

template<typename... Rs>
class functor_based_typed_actor : public typed_event_based_actor<Rs...> {

    typedef typed_event_based_actor<Rs...> super;

 public:

    typedef typed_event_based_actor<Rs...>* pointer;
    typedef typename super::behavior_type   behavior_type;

    typedef std::function<behavior_type ()>        no_arg_fun;
    typedef std::function<behavior_type (pointer)> one_arg_fun1;
    typedef std::function<void (pointer)>          one_arg_fun2;

    template<typename F, typename... Ts>
    functor_based_typed_actor(F fun, Ts&&... args) {
        typedef typename util::get_callable_trait<F>::type trait;
        typedef typename trait::arg_types arg_types;
        typedef typename trait::result_type result_type;
        constexpr bool returns_behavior = std::is_same<
                                              result_type,
                                              behavior_type
                                          >::value;
        constexpr bool uses_first_arg = std::is_same<
                                            typename util::tl_head<
                                                arg_types
                                            >::type,
                                            pointer
                                        >::value;
        std::integral_constant<bool, returns_behavior> token1;
        std::integral_constant<bool, uses_first_arg>   token2;
        set(token1, token2, std::move(fun), std::forward<Ts>(args)...);
    }

 protected:

    behavior_type make_behavior() override {
        return m_fun(this);
    }

 private:

    template<typename F>
    void set(std::true_type, std::true_type, F&& fun) {
        // behavior_type (pointer)
        m_fun = std::forward<F>(fun);
    }

    template<typename F>
    void set(std::false_type, std::true_type, F fun) {
        // void (pointer)
        m_fun = [fun](pointer ptr) {
            fun(ptr);
            return behavior_type{};
        };
    }

    template<typename F>
    void set(std::true_type, std::false_type, F fun) {
        // behavior_type ()
        m_fun = [fun](pointer) { return fun(); };
    }

    // (false_type, false_type) is an invalid functor for typed actors

    template<class Token, typename F, typename T0, typename... Ts>
    void set(Token t1, std::true_type t2, F fun, T0&& arg0, Ts&&... args) {
        set(t1, t2, std::bind(fun, std::placeholders::_1,
                              std::forward<T0>(arg0),
                              std::forward<Ts>(args)...));
    }

    template<class Token, typename F, typename T0, typename... Ts>
    void set(Token t1, std::false_type t2, F fun, T0&& arg0,  Ts&&... args) {
        set(t1, t2, std::bind(fun, std::forward<T0>(arg0),
                              std::forward<Ts>(args)...));
    }

    one_arg_fun1 m_fun;

};

template<class TypedBehavior, class FirstArg>
struct infer_typed_actor_base;

template<typename... Rs, class FirstArg>
struct infer_typed_actor_base<typed_behavior<Rs...>, FirstArg> {
    typedef functor_based_typed_actor<Rs...> type;
};

template<typename... Rs>
struct infer_typed_actor_base<void, typed_event_based_actor<Rs...>*> {
    typedef functor_based_typed_actor<Rs...> type;
};

} // namespace detail

/**
 * @brief Spawns a typed actor of type @p C.
 * @param args Constructor arguments.
 * @tparam C Subtype of {@link typed_event_based_actor}.
 * @tparam Os Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns A {@link typed_actor} handle to the spawned actor.
 */
template<class C, spawn_options Os, typename... Ts>
typename detail::actor_handle_from_signature_list<
    typename C::signatures
>::type
spawn_typed(Ts&&... args) {
    return detail::spawn_fwd_args<C, Os>(
            [&](C*) { },
            std::forward<Ts>(args)...);
}

/**
 * @brief Spawns a typed actor from a functor.
 * @param args A functor followed by its arguments.
 * @tparam Os Optional flags to modify <tt>spawn</tt>'s behavior.
 * @returns An {@link actor} to the spawned {@link actor}.
 */
template<spawn_options Os, typename F, typename... Ts>
typename detail::infer_typed_actor_handle<
    typename util::get_callable_trait<F>::result_type,
    typename util::tl_head<
        typename util::get_callable_trait<F>::arg_types
    >::type
>::type
spawn_typed(F fun, Ts&&... args) {
    typedef typename detail::infer_typed_actor_base<
                typename util::get_callable_trait<F>::result_type,
                typename util::tl_head<
                    typename util::get_callable_trait<F>::arg_types
                >::type
            >::type
            impl;
    return detail::spawn_fwd_args<impl, Os>(
            [&](impl*) { },
            std::move(fun), std::forward<Ts>(args)...);
}

/** @} */

} // namespace cppa

#endif // CPPA_SPAWN_HPP
