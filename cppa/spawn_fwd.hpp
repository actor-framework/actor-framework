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

template<class Impl, spawn_options Options = no_spawn_options, typename... Ts>
actor spawn(Ts&&... args);

template<spawn_options Options = no_spawn_options, typename... Ts>
actor spawn(Ts&&... args);

template<class Impl, spawn_options Options = no_spawn_options, typename... Ts>
actor spawn_in_group(const group&, Ts&&... args);

template<spawn_options Options = no_spawn_options, typename... Ts>
actor spawn_in_group(const group&, Ts&&... args);

/******************************************************************************
 *                                typed actors                                *
 ******************************************************************************/

namespace detail { // some utility

template<typename TypedBehavior>
struct actor_handle_from_typed_behavior;

template<typename... Signatures>
struct actor_handle_from_typed_behavior<typed_behavior<Signatures...>> {
    typedef typed_actor<Signatures...> type;
};

template<typename SignatureList>
struct actor_handle_from_signature_list;

template<typename... Signatures>
struct actor_handle_from_signature_list<util::type_list<Signatures...>> {
    typedef typed_actor<Signatures...> type;
};

} // namespace detail

template<spawn_options Options = no_spawn_options, typename F>
typename detail::actor_handle_from_typed_behavior<
    typename util::get_callable_trait<F>::result_type
>::type
spawn_typed(F fun);

template<class Impl, spawn_options Options = no_spawn_options, typename... Ts>
typename detail::actor_handle_from_signature_list<
    typename Impl::signatures
>::type
spawn_typed(Ts&&... args);

} // namespace cppa

#endif // CPPA_SPAWN_FWD_HPP
