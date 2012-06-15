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


#ifndef CPPA_FORWARD_ITERATOR_HPP
#define CPPA_FORWARD_ITERATOR_HPP

#include <iterator>

namespace cppa { namespace intrusive {

/**
 * @brief A forward iterator for intrusive lists.
 */
template<class T>
class forward_iterator {

 public:

    typedef T                           value_type;
    typedef value_type&                 reference;
    typedef const value_type&           const_reference;
    typedef value_type*                 pointer;
    typedef const value_type*           const_pointer;
    typedef ptrdiff_t                   difference_type;
    typedef std::forward_iterator_tag   iterator_category;

    inline forward_iterator(pointer ptr = nullptr) : m_ptr(ptr) { }

    forward_iterator(const forward_iterator&) = default;
    forward_iterator& operator=(const forward_iterator&) = default;

    inline forward_iterator& operator++() {
        m_ptr = m_ptr->next;
        return *this;
    }

    inline forward_iterator operator++(int) {
        forward_iterator tmp{*this};
        m_ptr = m_ptr->next;
        return tmp;
    }

    inline explicit operator bool() { return m_ptr != nullptr; }

    inline bool operator!() { return m_ptr != nullptr; }

    inline reference operator*() { return *m_ptr; }
    inline const_reference operator*() const { return *m_ptr; }

    inline pointer operator->() { return m_ptr; }
    inline const_pointer operator->() const { return m_ptr; }

    /**
     * @brief Returns the element this iterator points to.
     */
    inline pointer ptr() { return m_ptr; }

    /**
     * @brief Returns the element this iterator points to.
     */
    inline const_pointer ptr() const { return m_ptr; }

 private:

    pointer m_ptr;

};

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator==(const forward_iterator<T>& lhs,
                       const forward_iterator<T>& rhs) {
    return lhs.ptr() == rhs.ptr();
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator==(const forward_iterator<T>& lhs, const T* rhs) {
    return lhs.ptr() == rhs;
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator==(const T* lhs, const forward_iterator<T>& rhs) {
    return lhs == rhs.ptr();
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator==(const forward_iterator<T>& lhs, std::nullptr_t) {
    return lhs.ptr() == nullptr;
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator==(std::nullptr_t, const forward_iterator<T>& rhs) {
    return rhs.ptr() == nullptr;
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator!=(const forward_iterator<T>& lhs,
                       const forward_iterator<T>& rhs) {
    return !(lhs == rhs);
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator!=(const forward_iterator<T>& lhs, const T* rhs) {
    return !(lhs == rhs);
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator!=(const T* lhs, const forward_iterator<T>& rhs) {
    return !(lhs == rhs);
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator!=(const forward_iterator<T>& lhs, std::nullptr_t) {
    return !(lhs == nullptr);
}

/**
 * @relates forward_iterator
 */
template<class T>
inline bool operator!=(std::nullptr_t, const forward_iterator<T>& rhs) {
    return !(nullptr == rhs);
}

} } // namespace cppa::intrusive


#endif // CPPA_FORWARD_ITERATOR_HPP
