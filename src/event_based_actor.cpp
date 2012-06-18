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


#include "cppa/event_based_actor.hpp"

namespace cppa {

event_based_actor::event_based_actor() {
}

void event_based_actor::quit(std::uint32_t reason) {
    if (reason == exit_reason::normal) {
        cleanup(exit_reason::normal);
        m_behavior_stack.clear();
    }
    else {
        abstract_scheduled_actor::quit(reason);
    }
}

void event_based_actor::do_become(behavior* bhvr, bool has_ownership) {
    reset_timeout();
    request_timeout(bhvr->timeout());
    if (m_behavior_stack.empty() == false) {
        m_erased_stack_elements.emplace_back(std::move(m_behavior_stack.back()));
        m_behavior_stack.pop_back();
    }
    stack_element new_element{bhvr};
    if (!has_ownership) new_element.get_deleter().disable();
    m_behavior_stack.push_back(std::move(new_element));
}

} // namespace cppa
