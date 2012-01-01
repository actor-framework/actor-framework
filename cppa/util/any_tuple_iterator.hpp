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


#ifndef ANY_TUPLE_ITERATOR_HPP
#define ANY_TUPLE_ITERATOR_HPP

#include "cppa/any_tuple.hpp"

namespace cppa { namespace util {

class any_tuple_iterator
{

    any_tuple const& m_data;
    size_t m_pos;

 public:

    any_tuple_iterator(any_tuple const& data, size_t pos = 0);

    inline bool at_end() const;

    template<typename T>
    inline T const& value() const;

    inline void const* value_ptr() const;

    inline cppa::uniform_type_info const& type() const;

    inline size_t position() const;

    inline void next();

};

inline bool any_tuple_iterator::at_end() const
{
    return m_pos >= m_data.size();
}

template<typename T>
inline T const& any_tuple_iterator::value() const
{
    return *reinterpret_cast<T const*>(m_data.at(m_pos));
}

inline uniform_type_info const& any_tuple_iterator::type() const
{
    return m_data.utype_info_at(m_pos);
}

inline void const* any_tuple_iterator::value_ptr() const
{
    return m_data.at(m_pos);
}

inline size_t any_tuple_iterator::position() const
{
    return m_pos;
}

inline void any_tuple_iterator::next()
{
    ++m_pos;
}


} } // namespace cppa::util

#endif // ANY_TUPLE_ITERATOR_HPP
