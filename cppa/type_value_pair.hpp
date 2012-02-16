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


#ifndef TYPE_VALUE_PAIR_HPP
#define TYPE_VALUE_PAIR_HPP

#include "cppa/uniform_type_info.hpp"

namespace cppa {

typedef std::pair<uniform_type_info const*, void const*> type_value_pair;

class type_value_pair_const_iterator
{

    type_value_pair const* iter;

 public:

    type_value_pair_const_iterator(type_value_pair const* i) : iter(i) { }

    type_value_pair_const_iterator(type_value_pair_const_iterator const&) = default;

    inline uniform_type_info const* type() const { return iter->first; }

    inline void const* value() const { return iter->second; }

    inline decltype(iter) operator->() { return iter; }

    inline decltype(*iter) operator*() { return *iter; }

    inline type_value_pair_const_iterator& operator++()
    {
        ++iter;
        return *this;
    }

    inline type_value_pair_const_iterator& operator--()
    {
        --iter;
        return *this;
    }

    inline type_value_pair_const_iterator operator+(size_t offset)
    {
        return iter + offset;
    }

    inline bool operator==(type_value_pair_const_iterator const& other) const
    {
        return iter == other.iter;
    }

    inline bool operator!=(type_value_pair_const_iterator const& other) const
    {
        return iter != other.iter;
    }

};

} // namespace cppa

#endif // TYPE_VALUE_PAIR_HPP
