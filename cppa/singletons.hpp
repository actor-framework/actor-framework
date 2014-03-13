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


#ifndef CPPA_SINGLETONS_HPP
#define CPPA_SINGLETONS_HPP

#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

inline logging* get_logger() {
    return detail::singleton_manager::get_logger();
}

inline scheduler::coordinator* get_scheduling_coordinator() {
    return detail::singleton_manager::get_scheduling_coordinator();
}

inline detail::group_manager* get_group_manager() {
    return detail::singleton_manager::get_group_manager();
}

inline detail::actor_registry* get_actor_registry() {
    return detail::singleton_manager::get_actor_registry();
}

inline io::middleman* get_middleman() {
    return detail::singleton_manager::get_middleman();
}

inline detail::uniform_type_info_map* get_uniform_type_info_map() {
    return detail::singleton_manager::get_uniform_type_info_map();
}

inline detail::abstract_tuple* get_tuple_dummy() {
    return detail::singleton_manager::get_tuple_dummy();
}

inline detail::empty_tuple* get_empty_tuple() {
    return detail::singleton_manager::get_empty_tuple();
}

} // namespace cppa

#endif // CPPA_SINGLETONS_HPP
