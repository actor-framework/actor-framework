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


#include "cppa/any_tuple.hpp"
#include "cppa/detail/empty_tuple.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace {

struct offset_decorator : cppa::detail::abstract_tuple
{

    typedef cppa::cow_ptr<cppa::detail::abstract_tuple> ptr_type;

    offset_decorator(const ptr_type& decorated, size_t offset)
        : m_offset(offset)
        , m_decorated(decorated)
    {
    }

    void* mutable_at(size_t pos)
    {
        return m_decorated->mutable_at(pos + m_offset);
    }

    size_t size() const
    {
        return m_decorated->size() - m_offset;
    }

    abstract_tuple* copy() const
    {
        return new offset_decorator(m_decorated, m_offset);
    }

    const void* at(size_t pos) const
    {
        return m_decorated->at(pos + m_offset);
    }

    const cppa::uniform_type_info& utype_info_at(size_t pos) const
    {
        return m_decorated->utype_info_at(pos + m_offset);
    }

 private:

    size_t m_offset;
    ptr_type m_decorated;

};

inline cppa::detail::empty_tuple* s_empty_tuple()
{
    return cppa::detail::singleton_manager::get_empty_tuple();
}

} // namespace <anonymous>

namespace cppa {

any_tuple::any_tuple() : m_vals(s_empty_tuple())
{
}

any_tuple::any_tuple(detail::abstract_tuple* ptr) : m_vals(ptr)
{
}

any_tuple::any_tuple(any_tuple&& other) : m_vals(s_empty_tuple())
{
    m_vals.swap(other.m_vals);
}

any_tuple::any_tuple(const cow_ptr<detail::abstract_tuple>& vals) : m_vals(vals)
{
}

any_tuple& any_tuple::operator=(any_tuple&& other)
{
    m_vals.swap(other.m_vals);
    return *this;
}

any_tuple any_tuple::tail(size_t offset) const
{
    if (offset == 0) return *this;
    if (size() <= offset) return any_tuple(s_empty_tuple());
    return any_tuple(new offset_decorator(m_vals, offset));
}

size_t any_tuple::size() const
{
    return m_vals->size();
}

void* any_tuple::mutable_at(size_t p)
{
    return m_vals->mutable_at(p);
}

const void* any_tuple::at(size_t p) const
{
    return m_vals->at(p);
}

const uniform_type_info& any_tuple::utype_info_at(size_t p) const
{
    return m_vals->utype_info_at(p);
}

const cow_ptr<detail::abstract_tuple>& any_tuple::vals() const
{
    return m_vals;
}

bool any_tuple::equals(const any_tuple& other) const
{
    return m_vals->equals(*other.vals());
}

} // namespace cppa
