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


#ifndef DOUBLY_LINKED_LIST_HPP
#define DOUBLY_LINKED_LIST_HPP

#include <cstddef>
#include <utility>

#include "cppa/config.hpp"
#include "cppa/intrusive/bidirectional_iterator.hpp"

namespace cppa { namespace intrusive {

/**
 * @brief A singly linked list similar to std::forward_list
 *        but intrusive and with push_back() support.
 * @tparam T A class providing a @p next pointer and a default constructor.
 */
template<class T>
class doubly_linked_list
{

    doubly_linked_list(doubly_linked_list const&) = delete;
    doubly_linked_list& operator=(doubly_linked_list const&) = delete;

 public:

    typedef T                   value_type;
    typedef size_t              size_type;
    typedef ptrdiff_t           difference_type;
    typedef value_type&         reference;
    typedef value_type const&   const_reference;
    typedef value_type*         pointer;
    typedef value_type const*   const_pointer;

    typedef bidirectional_iterator<value_type>       iterator;
    typedef bidirectional_iterator<const value_type> const_iterator;

    doubly_linked_list() : m_head(), m_tail()
    {
        init();
    }

    doubly_linked_list(doubly_linked_list&& other) : m_head(), m_tail()
    {
        init();
        *this = std::move(other);
    }

    doubly_linked_list& operator=(doubly_linked_list&& other)
    {
        clear();
        if (other.not_empty())
        {
            connect(head(), other.first());
            connect(other.last(), tail());
            other.init();
        }
        return *this;
    }

    /**
     * @brief Creates a list from given [first, last] range.
     */
    static doubly_linked_list from(std::pair<pointer, pointer> const& p)
    {
        doubly_linked_list result;
        connect(result.head(), p.first);
        connect(p.second, result.tail());
        return result;
    }

    ~doubly_linked_list() { clear(); }

    // element access

    inline reference front() { return *first(); }
    inline const_reference front() const { return *first(); }

    inline reference back() { return *last(); }
    inline const_reference back() const { return *last(); }

    // iterators

    inline iterator begin() { return first(); }
    inline const_iterator begin() const { return first(); }
    inline const_iterator cbegin() const { return first(); }

    inline iterator end() { return tail(); }
    inline const_iterator end() const { return tail(); }
    inline const_iterator cend() const { return tail(); }

    // capacity

    /**
     * @brief Returns true if <tt>{@link begin()} == {@link end()}</tt>.
     */
    inline bool empty() const { return first() == tail(); }
    /**
     * @brief Returns true if <tt>{@link begin()} != {@link end()}</tt>.
     */
    inline bool not_empty() const { return first() != tail(); }

    // no size member function because it would have O(n) complexity

    // modifiers

    /**
     * @brief Deletes all elements.
     * @post {@link empty()}
     */
    void clear()
    {
        if (not_empty())
        {
            auto i = first();
            auto t = tail();
            while (i != t)
            {
                pointer next = i->next;
                delete i;
                i = next;
            }
            init();
        }
    }

    /**
     * @brief Inserts @p what after @p pos.
     * @returns An iterator to the inserted element.
     */
    iterator insert(iterator pos, pointer what)
    {
        connect(pos->prev, what);
        connect(what, pos.ptr());
        return what;
    }

    /**
     * @brief Constructs an element from @p args in-place after @p pos.
     * @returns An iterator to the inserted element.
     */
    template<typename... Args>
    iterator emplace(iterator pos, Args&&... args)
    {
        return insert(pos, new value_type(std::forward<Args>(args)...));
    }

    /**
     * @brief Deletes the element after @p pos.
     * @returns An iterator to the element following the erased one.
     */
    iterator erase(iterator pos)
    {
        CPPA_REQUIRE(pos != nullptr);
        CPPA_REQUIRE(pos != end());
        auto prev = pos->prev;
        auto next = pos->next;
        connect(prev, next);
        delete pos.ptr();
        return next;
    }

    /**
     * @brief Removes the element after @p pos from the list and returns it.
     */
    pointer take(iterator pos)
    {
        CPPA_REQUIRE(pos != nullptr);
        connect(pos->prev, pos->next);
        return pos.ptr();
    }

    /**
     * @brief Appends @p what to the list.
     */
    void push_back(pointer what)
    {
        connect(last(), what);
        connect(what, tail());
    }

    /**
     * @brief Creates an element from @p args in-place and appends it
     *        to the list.
     */
    template<typename... Args>
    void emplace_back(Args&&... args)
    {
        push_back(new value_type(std::forward<Args>(args)...));
    }

    /**
     * @brief Inserts @p what as the first element of the list.
     */
    void push_front(pointer what)
    {
        connect(what, first());
        connect(head(), what);
    }

    /**
     * @brief Creates an element from @p args and inserts it
     *        as the first element of the list.
     */
    template<typename... Args>
    void emplace_front(Args&&... args)
    {
        push_front(new value_type(std::forward<Args>(args)...));
    }

    /**
     * @brief Deletes the first element of the list.
     */
    void pop_front()
    {
        if (not_empty()) erase(first());
    }

    void pop_back()
    {
        if (not_empty()) erase(last());
    }

    /**
     * @brief Returns the content of the list as [first, last] sequence.
     * @post {@link empty()}
     */
    std::pair<pointer, pointer> take()
    {
        auto result = std::make_pair(first(), last());
        init();
        return result;
    }

    /**
     * @brief Moves all elements from @p other to @p *this.
     *        The elements are inserted after @p pos.
     * @pre <tt>@p pos != {@link end()}</tt>
     * @pre <tt>this != &other</tt>
     */
    void splice(iterator pos, doubly_linked_list&& other)
    {
        CPPA_REQUIRE(pos != nullptr);
        CPPA_REQUIRE(this != &other);
        if (empty())
        {
            *this = std::move(other);
        }
        else if (other.not_empty())
        {
            connect(last(), other.first());
            connect(other.last(), tail());
            other.init();
        }
    }

    /**
     * @brief Removes all elements for which predicate @p p returns @p true.
     */
    template<typename UnaryPredicate>
    void remove_if(UnaryPredicate p)
    {
        for (auto i = begin(); i != end(); )
        {
            if (p(*i))
            {
                i = erase(i);
            }
            else
            {
                ++i;
            }
        }
    }

    /**
     * @brief Removes all elements that are equal to @p value.
     */
    void remove(value_type const& value)
    {
        remove_if([&](value_type const& other) { return value == other; });
    }

 private:

    value_type m_head;
    value_type m_tail;

    inline value_type* head() { return &m_head; }
    inline value_type const* head() const { return &m_head; }

    inline value_type* first() { return m_head.next; }
    inline value_type const* first() const { return m_head.next; }

    inline value_type* tail() { return &m_tail; }
    inline value_type const* tail() const { return &m_tail; }

    inline value_type* last() { return m_tail.prev; }
    inline value_type const* last() const { return m_tail.prev; }

    static inline void connect(value_type* lhs, value_type* rhs)
    {
        lhs->next = rhs;
        rhs->prev = lhs;
    }

    inline void init()
    {
        connect(&m_head, &m_tail);
    }

};

} } // namespace cppa::intrusive

#endif // DOUBLY_LINKED_LIST_HPP
