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


#ifndef SINGLY_LINKED_LIST_HPP
#define SINGLY_LINKED_LIST_HPP

#include <utility>
#include "cppa/util/default_deallocator.hpp"

namespace cppa { namespace util {

template<typename T, class Deallocator = default_deallocator<T> >
class singly_linked_list
{

    Deallocator d;
    T* m_head;
    T* m_tail;

 public:

    typedef T element_type;

    singly_linked_list() : m_head(nullptr), m_tail(nullptr) { }

    ~singly_linked_list()
    {
        clear();
    }

    inline bool empty() const { return m_head == nullptr; }

    void push_back(element_type* what)
    {
        what->next = nullptr;
        if (m_tail)
        {
            m_tail->next = what;
            m_tail = what;
        }
        else
        {
            m_head = m_tail = what;
        }
    }

    std::pair<element_type*, element_type*> take()
    {
        element_type* first = m_head;
        element_type* last = m_tail;
        m_head = m_tail = nullptr;
        return { first, last };
    }

    void clear()
    {
        while (m_head)
        {
            T* next = m_head->next;
            d(m_head);
            m_head = next;
        }
        m_head = m_tail = nullptr;
    }

};

} } // namespace cppa::util

#endif // SINGLY_LINKED_LIST_HPP
