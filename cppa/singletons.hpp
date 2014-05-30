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

} // namespace cppa

#endif // CPPA_SINGLETONS_HPP
