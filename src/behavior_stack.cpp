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


#include "cppa/local_actor.hpp"
#include "cppa/detail/behavior_stack.hpp"

namespace cppa { namespace detail {

// executes the behavior stack
void behavior_stack::exec() {
    while (!empty()) {
        self->dequeue(back());
        cleanup();
    }
}

void behavior_stack::pop_back() {
    if (m_elements.empty() == false) {
        m_erased_elements.emplace_back(std::move(m_elements.back()));
        m_elements.pop_back();
    }
}

void behavior_stack::push_back(behavior* what, bool has_ownership) {
    value_type new_element{what};
    if (!has_ownership) new_element.get_deleter().disable();
    m_elements.push_back(std::move(new_element));
}

void behavior_stack::clear() {
    if (m_elements.empty() == false) {
        std::move(m_elements.begin(), m_elements.end(),
                  std::back_inserter(m_erased_elements));
        m_elements.clear();
    }
}

void behavior_stack::cleanup() {
    m_erased_elements.clear();
}

} } // namespace cppa::detail
