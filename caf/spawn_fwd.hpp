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

// this header contains prototype definitions of the spawn function famility;
// implementations can be found in spawn.hpp (this header is included there)

#ifndef CAF_SPAWN_FWD_HPP
#define CAF_SPAWN_FWD_HPP

#include "caf/group.hpp"
#include "caf/typed_actor.hpp"
#include "caf/spawn_options.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {

template<class C,
         spawn_options Os = no_spawn_options,
         typename BeforeLaunch = std::function<void (C*)>,
         typename... Ts>
intrusive_ptr<C> spawn_class(execution_unit* host,
                             BeforeLaunch before_launch_fun, Ts&&... args);

template<spawn_options Os = no_spawn_options,
          typename BeforeLaunch = void (*)(abstract_actor*),
          typename F = behavior (*)(), typename... Ts>
actor spawn_functor(execution_unit* host, BeforeLaunch before_launch_fun, F fun,
                    Ts&&... args);

class group_subscriber {

 public:

    inline group_subscriber(const group& grp) : m_grp(grp) {}

    template<typename T>
    inline void operator()(T* ptr) const {
        ptr->join(m_grp);
    }

 private:

    group m_grp;

};

class empty_before_launch_callback {

 public:

    template<typename T>
    inline void operator()(T*) const {}

};

/******************************************************************************
 *                                typed actors                                *
 ******************************************************************************/

namespace detail { // some utility

template<class TypedBehavior, class FirstArg>
struct infer_typed_actor_handle;

// infer actor type from result type if possible
template<typename... Rs, class FirstArg>
struct infer_typed_actor_handle<typed_behavior<Rs...>, FirstArg> {
    using type = typed_actor<Rs...>;

};

// infer actor type from first argument if result type is void
template<typename... Rs>
struct infer_typed_actor_handle<void, typed_event_based_actor<Rs...>*> {
    using type = typed_actor<Rs...>;

};

template<typename SignatureList>
struct actor_handle_from_signature_list;

template<typename... Rs>
struct actor_handle_from_signature_list<detail::type_list<Rs...>> {
    using type = typed_actor<Rs...>;

};

} // namespace detail

template<spawn_options Os, typename BeforeLaunch, typename F, typename... Ts>
typename detail::infer_typed_actor_handle<
    typename detail::get_callable_trait<F>::result_type,
    typename detail::tl_head<
        typename detail::get_callable_trait<F>::arg_types>::type>::type
spawn_typed_functor(execution_unit*, BeforeLaunch bl, F fun, Ts&&... args);

} // namespace caf

#endif // CAF_SPAWN_FWD_HPP
