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


#ifndef CPPA_FACTORY_HPP
#define CPPA_FACTORY_HPP

#include "cppa/detail/event_based_actor_factory.hpp"

namespace cppa { namespace factory {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Returns a factory for event-based actors using @p fun as
 *        implementation for {@link cppa::event_based_actor::init() init()}.
 *
 * @p fun must take pointer arguments only. The factory creates an event-based
 * actor implementation with member variables according to the functor's
 * signature, as shown in the example below.
 *
 * @code
 * auto f = factory::event_based([](int* a, int* b) { ... });
 * auto actor1 = f.spawn();
 * auto actor2 = f.spawn(1);
 * auto actor3 = f.spawn(1, 2);
 * @endcode
 *
 * The arguments @p a and @p b will point to @p int member variables of the
 * actor. All member variables are initialized using the default constructor
 * unless an initial value is passed to @p spawn.
 */
template<typename InitFun>
auto event_based(InitFun fun);

/**
 * @brief Returns a factory for event-based actors using @p fun0 as
 *        implementation for {@link cppa::event_based_actor::init() init()}
 *        and @p fun1 as implementation for
 *        {@link cppa::event_based_actor::on_exit() on_exit()}.
 */
template<typename InitFun, OnExitFun>
auto event_based(InitFun fun0, OnExitFun fun1);

#else // CPPA_DOCUMENTATION

void default_cleanup();

template<typename InitFun>
inline typename detail::ebaf_from_functor<InitFun, void (*)()>::type
event_based(InitFun init) {
    return {std::move(init), default_cleanup};
}

template<typename InitFun, typename OnExitFun>
inline typename detail::ebaf_from_functor<InitFun, OnExitFun>::type
event_based(InitFun init, OnExitFun on_exit) {
    return {std::move(init), on_exit};
}

#endif // CPPA_DOCUMENTATION

} } // namespace cppa::factory

#endif // CPPA_FACTORY_HPP
