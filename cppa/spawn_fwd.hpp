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


// this header contains prototype definitions of the spawn function famility;
// implementations can be found in spawn.hpp (this header is included there)

#ifndef CPPA_SPAWN_FWD_HPP
#define CPPA_SPAWN_FWD_HPP

#include "cppa/typed_actor.hpp"
#include "cppa/spawn_options.hpp"

#include "cppa/util/type_list.hpp"

namespace cppa {

/******************************************************************************
 *                               untyped actors                               *
 ******************************************************************************/

template<class Impl, spawn_options Os = no_spawn_options, typename... Ts>
actor spawn(Ts&&... args);

template<spawn_options Os = no_spawn_options, typename... Ts>
actor spawn(Ts&&... args);

template<class Impl, spawn_options Os = no_spawn_options, typename... Ts>
actor spawn_in_group(const group&, Ts&&... args);

template<spawn_options Os = no_spawn_options, typename... Ts>
actor spawn_in_group(const group&, Ts&&... args);

/******************************************************************************
 *                                typed actors                                *
 ******************************************************************************/

namespace detail { // some utility

template<class TypedBehavior, class FirstArg>
struct infer_typed_actor_handle;

// infer actor type from result type if possible
template<typename... Rs, class FirstArg>
struct infer_typed_actor_handle<typed_behavior<Rs...>, FirstArg> {
    typedef typed_actor<Rs...> type;
};

// infer actor type from first argument if result type is void
template<typename... Rs>
struct infer_typed_actor_handle<void, typed_event_based_actor<Rs...>*> {
    typedef typed_actor<Rs...> type;
};

template<typename SignatureList>
struct actor_handle_from_signature_list;

template<typename... Rs>
struct actor_handle_from_signature_list<util::type_list<Rs...>> {
    typedef typed_actor<Rs...> type;
};

} // namespace detail

template<class Impl, spawn_options Os = no_spawn_options, typename... Ts>
typename detail::actor_handle_from_signature_list<
    typename Impl::signatures
>::type
spawn_typed(Ts&&... args);

template<spawn_options Os = no_spawn_options, typename F, typename... Ts>
typename detail::infer_typed_actor_handle<
    typename util::get_callable_trait<F>::result_type,
    typename util::tl_head<
        typename util::get_callable_trait<F>::arg_types
    >::type
>::type
spawn_typed(F fun, Ts&&... args);

} // namespace cppa

#endif // CPPA_SPAWN_FWD_HPP
