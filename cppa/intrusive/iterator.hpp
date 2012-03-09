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


#ifndef ITERATOR_HPP
#define ITERATOR_HPP

#include <iterator>

namespace cppa { namespace intrusive {

template<class T>
class iterator // : std::iterator<forward_iterator_tag, T>
{

 public:

    typedef T                           value_type;
    typedef value_type&                 reference;
    typedef value_type const&           const_reference;
    typedef value_type*                 pointer;
    typedef value_type const*           const_pointer;
    typedef ptrdiff_t                   difference_type;
    typedef std::forward_iterator_tag   iterator_category;

    inline iterator(T* ptr) : m_ptr(ptr) { }

    iterator(iterator const&) = default;
    iterator& operator=(iterator const&) = default;

    inline iterator& operator++()
    {
        m_ptr = m_ptr->next;
        return *this;
    }

    inline iterator operator++(int)
    {
        iterator tmp{*this};
        m_ptr = m_ptr->next;
        return tmp;
    }

    inline reference operator*() { return *m_ptr; }
    inline const_reference operator*() const { return *m_ptr; }

    inline pointer operator->() { return m_ptr; }
    inline const_pointer operator->() const { return m_ptr; }

    inline pointer ptr() { return m_ptr; }
    inline const_pointer ptr() const { return m_ptr; }

 private:

    pointer m_ptr;

};

template<class T>
inline bool operator==(iterator<T> const& lhs, iterator<T> const& rhs)
{
    return lhs.ptr() == rhs.ptr();
}

template<class T>
inline bool operator==(iterator<T> const& lhs, T const* rhs)
{
    return lhs.ptr() == rhs;
}

template<class T>
inline bool operator==(T const* lhs, iterator<T> const& rhs)
{
    return lhs == rhs.ptr();
}

template<class T>
inline bool operator==(iterator<T> const& lhs, decltype(nullptr))
{
    return lhs.ptr() == nullptr;
}

template<class T>
inline bool operator==(decltype(nullptr), iterator<T> const& rhs)
{
    return rhs.ptr() == nullptr;
}

template<class T>
inline bool operator!=(iterator<T> const& lhs, iterator<T> const& rhs)
{
    return !(lhs == rhs);
}

template<class T>
inline bool operator!=(iterator<T> const& lhs, T const* rhs)
{
    return !(lhs == rhs);
}

template<class T>
inline bool operator!=(T const* lhs, iterator<T> const& rhs)
{
    return !(lhs == rhs);
}

template<class T>
inline bool operator!=(iterator<T> const& lhs, decltype(nullptr))
{
    return !(lhs == nullptr);
}

template<class T>
inline bool operator!=(decltype(nullptr), iterator<T> const& rhs)
{
    return !(nullptr == rhs);
}

} } // namespace cppa::intrusive


#endif // ITERATOR_HPP
