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


#include <iterator>
#include "cppa/local_actor.hpp"
#include "cppa/detail/behavior_stack.hpp"

namespace cppa { namespace detail {

class behavior_stack_help_iterator
: public std::iterator<std::output_iterator_tag, void, void, void, void> {

 public:

    typedef behavior_stack::element_type element_type;

    explicit behavior_stack_help_iterator(behavior_stack& st) : m_stack(&st) { }

    behavior_stack_help_iterator& operator=(element_type&& rval) {
        m_stack->m_erased_elements.emplace_back(std::move(rval.first));
        return *this;
    }

    behavior_stack_help_iterator& operator*() { return *this; }

    behavior_stack_help_iterator& operator++() { return *this; }

    behavior_stack_help_iterator operator++(int) { return *this; }

 private:

    behavior_stack* m_stack;

};

void behavior_stack::pop_async_back() {
    if (m_elements.empty()) {
        // nothing to do
    }
    else if (m_elements.back().second.valid() == false) {
        m_erased_elements.emplace_back(std::move(m_elements.back().first));
        m_elements.pop_back();
    }
    else {
        auto rlast = m_elements.rend();
        auto ri = std::find_if(m_elements.rbegin(), rlast,
                              [](element_type& e) {
                                  return e.second.valid() == false;
                              });
        if (ri != rlast) {
            m_erased_elements.emplace_back(std::move(ri->first));
            auto i = ri.base();
            --i; // adjust 'normal' iterator to point to the correct element
            m_elements.erase(i);
        }
    }
}

void behavior_stack::erase(message_id_t response_id) {
    auto last = m_elements.end();
    auto i = std::find_if(m_elements.begin(), last, [=](element_type& e) {
        return e.second == response_id;
    });
    if (i != last) {
        m_erased_elements.emplace_back(std::move(i->first));
        m_elements.erase(i);
    }
}

void behavior_stack::push_back(behavior&& what, message_id_t response_id) {
    m_elements.emplace_back(std::move(what), response_id);
}

void behavior_stack::clear() {
    if (m_elements.empty() == false) {
        std::move(m_elements.begin(), m_elements.end(),
                  behavior_stack_help_iterator{*this});
        m_elements.clear();
    }
}

void behavior_stack::cleanup() {
    m_erased_elements.clear();
}

} } // namespace cppa::detail
