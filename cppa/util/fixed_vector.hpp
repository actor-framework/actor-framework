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


#ifndef FIXED_VECTOR_HPP
#define FIXED_VECTOR_HPP

#include <algorithm>
#include <stdexcept>

namespace cppa { namespace util {

// @warning does not perform any bound checks
template<typename T, size_t MaxSize>
class fixed_vector
{

    size_t m_size;

    // support for MaxSize == 0
    T m_data[(MaxSize > 0) ? MaxSize : 1];

 public:

    typedef size_t size_type;

    typedef T& reference;
    typedef T const& const_reference;

    typedef T* iterator;
    typedef T const* const_iterator;

    constexpr fixed_vector() : m_size(0)
    {
    }

    fixed_vector(fixed_vector const& other) : m_size(other.m_size)
    {
        std::copy(other.m_data, other.m_data + other.m_size, m_data);
    }

    inline size_type size() const
    {
        return m_size;
    }

    inline void clear()
    {
        m_size = 0;
    }

    inline bool full() const
    {
        return m_size == MaxSize;
    }

    inline void push_back(T const& what)
    {
        m_data[m_size++] = what;
    }

    inline void push_back(T&& what)
    {
        m_data[m_size++] = std::move(what);
    }

    inline reference operator[](size_t pos)
    {
        return m_data[pos];
    }

    inline const_reference operator[](size_t pos) const
    {
        return m_data[pos];
    }

    inline iterator begin()
    {
        return m_data;
    }

    inline const_iterator begin() const
    {
        return m_data;
    }

    inline iterator end()
    {
        return (static_cast<T*>(m_data) + m_size);
    }

    inline const_iterator end() const
    {
        return (static_cast<T const*>(m_data) + m_size);
    }

    template<class InputIterator>
    inline void insert(iterator pos,
                       InputIterator first,
                       InputIterator last)
    {
        if (pos == end())
        {
            if (m_size + static_cast<size_type>(last - first) > MaxSize)
            {
                throw std::runtime_error("fixed_vector::insert: full");
            }
            for ( ; first != last; ++first)
            {
                push_back(*first);
            }
        }
        else
        {
            throw std::runtime_error("not implemented fixed_vector::insert");
        }
    }

};

} } // namespace cppa::util

#endif // FIXED_VECTOR_HPP
