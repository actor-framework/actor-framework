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


#include "cppa/detail/object_array.hpp"

namespace cppa { namespace detail {

object_array::object_array()
{
}

object_array::object_array(object_array&& other)
    : m_elements(std::move(other.m_elements))
{
}

object_array::object_array(object_array const& other)
    : m_elements(other.m_elements)
{
}

void object_array::push_back(object const& what)
{
    m_elements.push_back(what);
}

void object_array::push_back(object&& what)
{
    m_elements.push_back(std::move(what));
}

void* object_array::mutable_at(size_t pos)
{
    return m_elements[pos].mutable_value();
}

size_t object_array::size() const
{
    return m_elements.size();
}

abstract_tuple* object_array::copy() const
{
    return new object_array(*this);
}

void const* object_array::at(size_t pos) const
{
    return m_elements[pos].value();
}

bool object_array::equals(cppa::detail::abstract_tuple const& ut) const
{
    if (size() == ut.size())
    {
        for (size_t i = 0; i < size(); ++i)
        {
            auto utype = type_at(i);
            if (utype == ut.type_at(i))
            {
                if (!utype->equals(at(i), ut.at(i))) return false;
            }
            else
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

uniform_type_info const* object_array::type_at(size_t pos) const
{
    return m_elements[pos].type();
}

} } // namespace cppa::detail
