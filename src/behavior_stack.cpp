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


#include <iterator>

#include "cppa/local_actor.hpp"
#include "cppa/detail/behavior_stack.hpp"

using namespace std;

namespace cppa { namespace detail {

struct behavior_stack_mover : iterator<output_iterator_tag, void, void, void, void>{

 public:

    behavior_stack_mover(behavior_stack* st) : m_stack(st) { }

    behavior_stack_mover& operator=(behavior_stack::element_type&& rval) {
        m_stack->m_erased_elements.emplace_back(move(rval.first));
        return *this;
    }

    behavior_stack_mover& operator*() { return *this; }

    behavior_stack_mover& operator++() { return *this; }

    behavior_stack_mover operator++(int) { return *this; }

 private:

    behavior_stack* m_stack;

};

inline behavior_stack_mover move_iter(behavior_stack* bs) { return {bs}; }

optional<behavior&> behavior_stack::sync_handler(message_id expected_response) {
    if (expected_response.valid()) {
        auto e = m_elements.rend();
        auto i = find_if(m_elements.rbegin(), e, [=](element_type& val) {
            return val.second == expected_response;
        });
        if (i != e) return i->first;
    }
    return {};
}

void behavior_stack::pop_async_back() {
    if (m_elements.empty()) { } // nothing to do
    else if (!m_elements.back().second.valid()) erase_at(m_elements.end() - 1);
    else rerase_if([](const element_type& e) {
        return e.second.valid() == false;
    });
}

void behavior_stack::clear() {
    if (m_elements.empty() == false) {
        move(m_elements.begin(), m_elements.end(), move_iter(this));
        m_elements.clear();
    }
}

} } // namespace cppa::detail
