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


#ifndef CPPA_SINGLY_LINKED_LIST_HPP
#define CPPA_SINGLY_LINKED_LIST_HPP

#include <cstddef>
#include <utility>

#include "cppa/config.hpp"
#include "cppa/intrusive/forward_iterator.hpp"

namespace cppa { namespace intrusive {

/**
 * @brief A singly linked list similar to std::forward_list
 *        but intrusive and with push_back() support.
 * @tparam T A class providing a @p next pointer and a default constructor.
 */
template<class T>
class singly_linked_list {

    singly_linked_list(const singly_linked_list&) = delete;
    singly_linked_list& operator=(const singly_linked_list&) = delete;

 public:

    typedef T                   value_type;
    typedef size_t              size_type;
    typedef ptrdiff_t           difference_type;
    typedef value_type&         reference;
    typedef const value_type&   const_reference;
    typedef value_type*         pointer;
    typedef const value_type*   const_pointer;

    typedef forward_iterator<value_type>       iterator;
    typedef forward_iterator<value_type const> const_iterator;

    singly_linked_list() : m_head(), m_tail(&m_head) { }

    singly_linked_list(singly_linked_list&& other) : m_head(), m_tail(&m_head) {
        *this = std::move(other);
    }

    singly_linked_list& operator=(singly_linked_list&& other) {
        clear();
        if (other.not_empty()) {
            m_head.next = other.m_head.next;
            m_tail = other.m_tail;
            other.m_head.next = nullptr;
            other.m_tail = &(other.m_head);
        }
        return *this;
    }

    /**
     * @brief Creates a list from given [first, last] range.
     */
    static singly_linked_list from(const std::pair<pointer, pointer>& p) {
        singly_linked_list result;
        result.m_head.next = p.first;
        result.m_tail = p.second;
        return result;
    }

    ~singly_linked_list() { clear(); }

    // element access

    inline reference front() { return *(m_head.next); }
    inline const_reference front() const { return *(m_head.next); }

    inline reference back() { return *m_tail; }
    inline const_reference back() const { return *m_tail; }

    // iterators

    inline iterator before_begin() { return &m_head; }
    inline const_iterator before_begin() const { return &m_head; }
    inline const_iterator cbefore_begin() const { return &m_head; }

    inline iterator begin() { return m_head.next; }
    inline const_iterator begin() const { return m_head.next; }
    inline const_iterator cbegin() const { return m_head.next; }

    inline iterator before_end() { return m_tail; }
    inline const_iterator before_end() const { return m_tail; }
    inline const_iterator cbefore_end() const { return m_tail; }

    inline iterator end() { return nullptr; }
    inline const_iterator end() const { return nullptr; }
    inline const_iterator cend() const { return nullptr; }

    // capacity

    /**
     * @brief Returns true if <tt>{@link begin()} == {@link end()}</tt>.
     */
    inline bool empty() const { return m_head.next == nullptr; }
    /**
     * @brief Returns true if <tt>{@link begin()} != {@link end()}</tt>.
     */
    inline bool not_empty() const { return m_head.next != nullptr; }

    // no size member function because it would have O(n) complexity

    // modifiers

    /**
     * @brief Deletes all elements.
     * @post {@link empty()}
     */
    void clear() {
        if (not_empty()) {
            auto h = m_head.next;
            while (h) {
                pointer next = h->next;
                delete h;
                h = next;
            }
            m_head.next = nullptr;
            m_tail = &m_head;
        }
    }

    /**
     * @brief Inserts @p what after @p pos.
     * @returns An iterator to the inserted element.
     */
    iterator insert_after(iterator pos, pointer what) {
        what->next = pos->next;
        pos->next = what;
        if (pos == m_tail) m_tail = what;
        return what;
    }

    /**
     * @brief Constructs an element from @p args in-place after @p pos.
     * @returns An iterator to the inserted element.
     */
    template<typename... Args>
    void emplace_after(iterator pos, Args&&... args) {
        insert_after(pos, new value_type(std::forward<Args>(args)...));
    }

    /**
     * @brief Deletes the element after @p pos.
     * @returns An iterator to the element following the erased one.
     */
    iterator erase_after(iterator pos) {
        CPPA_REQUIRE(pos != nullptr);
        auto next = pos->next;
        if (next) {
            if (next == m_tail) m_tail = pos.ptr();
            pos->next = next->next;
            delete next;
        }
        return pos->next;
    }

    /**
     * @brief Removes the element after @p pos from the list and returns it.
     */
    pointer take_after(iterator pos) {
        CPPA_REQUIRE(pos != nullptr);
        auto next = pos->next;
        if (next) {
            if (next == m_tail) m_tail = pos.ptr();
            pos->next = next->next;
            next->next = nullptr;
        }
        return next;
    }

    /**
     * @brief Appends @p what to the list.
     */
    void push_back(pointer what) {
        what->next = nullptr;
        m_tail->next = what;
        m_tail = what;
    }

    /**
     * @brief Creates an element from @p args in-place and appends it
     *        to the list.
     */
    template<typename... Args>
    void emplace_back(Args&&... args) {
        push_back(new value_type(std::forward<Args>(args)...));
    }

    /**
     * @brief Inserts @p what as the first element of the list.
     */
    void push_front(pointer what) {
        if (empty()) {
            push_back(what);
        }
        else {
            what->next = m_head.next;
            m_head.next = what;
        }
    }

    /**
     * @brief Creates an element from @p args and inserts it
     *        as the first element of the list.
     */
    template<typename... Args>
    void emplace_front(Args&&... args) {
        push_front(new value_type(std::forward<Args>(args)...));
    }

    /**
     * @brief Deletes the first element of the list.
     */
    void pop_front() {
        auto x = m_head.next;
        if (x == nullptr) {
            // list is empty
            return;
        }
        else if (x == m_tail) {
            m_tail = &m_head;
            m_head.next = nullptr;
        }
        else {
            m_head.next = x->next;
        }
        delete x;
    }

    // pop_back would have complexity O(n)

    /**
     * @brief Returns the content of the list as [first, last] sequence.
     * @post {@link empty()}
     */
    std::pair<pointer, pointer> take() {
        if (empty()) {
            return {nullptr, nullptr};
        }
        else {
            auto result = std::make_pair(m_head.next, m_tail);
            m_head.next = nullptr;
            m_tail = &m_head;
            return result;
        }
    }

    /**
     * @brief Moves all elements from @p other to @p *this.
     *        The elements are inserted after @p pos.
     * @pre <tt>@p pos != {@link end()}</tt>
     * @pre <tt>this != &other</tt>
     */
    void splice_after(iterator pos, singly_linked_list&& other) {
        CPPA_REQUIRE(pos != nullptr);
        CPPA_REQUIRE(this != &other);
        if (other.not_empty()) {
            auto next = pos->next;
            auto pair = other.take();
            pos->next = pair.first;
            pair.second->next = next;
            if (pos == m_tail) {
                CPPA_REQUIRE(next == nullptr);
                m_tail = pair.second;
            }
        }
    }

    /**
     * @brief Removes all elements for which predicate @p p returns @p true.
     */
    template<typename UnaryPredicate>
    void remove_if(UnaryPredicate p) {
        auto i = before_begin();
        while (i->next != nullptr) {
            if (p(*(i->next))) { (void) erase_after(i);
            }
            else {
                ++i;
            }
        }
    }

    /**
     * @brief Removes the first element for which predicate @p p
     *        returns @p true.
     * @returns iterator to the element that preceded the removed element
     *          or end().
     */
    template<typename UnaryPredicate>
    iterator remove_first(UnaryPredicate p, iterator before_first) {
        CPPA_REQUIRE(before_first != end());
        while (before_first->next != nullptr) {
            if (p(*(before_first->next))) {
                erase_after(before_first);
                return before_first;
            }
            else {
                ++before_first;
            }
        }
        return end();
    }

    template<typename UnaryPredicate>
    inline iterator remove_first(UnaryPredicate p) {
        return remove_first(std::move(p), before_begin());
    }

    /**
     * @brief Removes all elements that are equal to @p value.
     */
    void remove(const value_type& value) {
        remove_if([&](const value_type& other) { return value == other; });
    }

 private:

    value_type m_head;
    pointer m_tail;

};

} } // namespace cppa::intrusive

#endif // CPPA_SINGLY_LINKED_LIST_HPP
