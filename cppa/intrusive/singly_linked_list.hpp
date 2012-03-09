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

#include <cstddef>
#include <utility>

#include "cppa/config.hpp"
#include "cppa/intrusive/iterator.hpp"

namespace cppa { namespace intrusive {

template<class T>
class singly_linked_list
{

    singly_linked_list(singly_linked_list const&) = delete;
    singly_linked_list& operator=(singly_linked_list const&) = delete;

 public:

    typedef T                   value_type;
    typedef size_t              size_type;
    typedef ptrdiff_t           difference_type;
    typedef value_type&         reference;
    typedef value_type const&   const_reference;
    typedef value_type*         pointer;
    typedef value_type const*   const_pointer;

    typedef ::cppa::intrusive::iterator<value_type>        iterator;
    typedef ::cppa::intrusive::iterator<value_type const>  const_iterator;

    singly_linked_list() : m_head(nullptr), m_tail(nullptr) { }

    singly_linked_list(singly_linked_list&& other)
        : m_head(other.m_head), m_tail(other.m_tail)
    {
        other.m_head = other.m_tail = nullptr;
    }

    singly_linked_list& operator=(singly_linked_list&& other)
    {
        std::swap(m_head, other.m_head);
        std::swap(m_tail, other.m_tail);
        return *this;
    }

    static singly_linked_list from(std::pair<pointer, pointer> const& p)
    {
        singly_linked_list result;
        result.m_head = p.first;
        result.m_tail = p.second;
        return result;
    }

    ~singly_linked_list() { clear(); }

    // element access

    inline reference front() { return *m_head; }
    inline const_reference front() const { return *m_head; }

    inline reference back() { return *m_tail; }
    inline const_reference back() const { return *m_tail; }

    // iterators

    inline iterator begin() { return m_head; }
    inline const_iterator begin() const { return m_head; }
    inline const_iterator cbegin() const { return m_head; }

    inline iterator before_end() { return m_tail; }
    inline const_iterator before_end() const { return m_tail; }
    inline const_iterator cbefore_end() const { return m_tail; }

    inline iterator end() { return nullptr; }
    inline const_iterator end() const { return nullptr; }
    inline const_iterator cend() const { return nullptr; }

    // capacity

    inline bool empty() const { return m_head == nullptr; }
    // no size member function because it would have O(n) complexity

    // modifiers

    void clear()
    {
        while (m_head)
        {
            pointer next = m_head->next;
            delete m_head;
            m_head = next;
        }
        m_tail = nullptr;
    }

    iterator insert_after(iterator i, pointer what)
    {
        what->next = i->next;
        i->next = what;
        return what;
    }

    template<typename... Args>
    void emplace_after(iterator i, Args&&... args)
    {
        insert_after(i, new value_type(std::forward<Args>(args)...));
    }

    iterator erase_after(iterator pos)
    {
        CPPA_REQUIRE(pos != nullptr);
        auto next = pos->next;
        if (next)
        {
            pos->next = next->next;
            delete next;
        }
        return pos->next;
    }

    void push_back(pointer what)
    {
        what->next = nullptr;
        if (empty())
        {
            m_tail = m_head = what;
        }
        else
        {
            m_tail->next = what;
            m_tail = what;
        }
    }

    template<typename... Args>
    void emplace_back(Args&&... args)
    {
        push_back(new value_type(std::forward<Args>(args)...));
    }

    // pop_back would have complexity O(n)

    void push_front(pointer what)
    {
        if (empty())
        {
            what->next = nullptr;
            m_tail = m_head = what;
        }
        else
        {
            what->next = m_head;
            m_head = what;
        }
    }

    template<typename... Args>
    void emplace_front(Args&&... args)
    {
        push_front(new value_type(std::forward<Args>(args)...));
    }

    void pop_front()
    {
        if (!empty())
        {
            auto next = m_head->next;
            delete m_head;
            m_head = next;
            if (m_head == nullptr) m_tail = nullptr;
        }
    }

    std::pair<pointer, pointer> take()
    {
        auto result = std::make_pair(m_head, m_tail);
        m_head = m_tail = nullptr;
        return result;
    }

    void splice_after(iterator pos, singly_linked_list&& other)
    {
        CPPA_REQUIRE(pos != nullptr);
        CPPA_REQUIRE(this != &other);
        if (other.empty() == false)
        {
            auto next = pos->next;
            auto pair = other.take();
            pos->next = pair.first;
            pair.second->next = next;
            if (pos == m_tail)
            {
                CPPA_REQUIRE(next == nullptr);
                m_tail = pair.second;
            }
        }
    }

    template<typename UnaryPredicate>
    void remove_if(UnaryPredicate p)
    {
        auto i = begin();
        while (!empty() && p(*i))
        {
            pop_front();
            i = begin();
        }
        if (empty()) return;
        auto predecessor = i;
        ++i;
        while (i != nullptr)
        {
            if (p(*i))
            {
                i = erase_after(predecessor);
            }
            else
            {
                predecessor = i;
                ++i;
            }
        }
    }

    void remove(value_type const& value)
    {
        remove_if([&](value_type const& other) { return value == other; });
    }

 private:

    pointer m_head;
    pointer m_tail;

};

} } // namespace cppa::intrusive

#endif // SINGLY_LINKED_LIST_HPP
