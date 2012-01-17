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


#ifndef TUPLE_VALS_HPP
#define TUPLE_VALS_HPP

#include <stdexcept>

#include "cppa/util/type_list.hpp"

#include "cppa/detail/tdata.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/serialize_tuple.hpp"

namespace cppa { namespace detail {

template<typename... ElementTypes>
class tuple_vals : public abstract_tuple
{

    typedef abstract_tuple super;

    typedef tdata<ElementTypes...> data_type;

    typedef types_array<ElementTypes...> element_types;

    data_type m_data;

    static types_array<ElementTypes...> m_types;

    template<typename... Types>
    void* tdata_mutable_at(tdata<Types...>& d, size_t pos)
    {
        return (pos == 0) ? &(d.head) : tdata_mutable_at(d.tail(), pos - 1);
    }

    template<typename... Types>
    void const* tdata_at(tdata<Types...> const& d, size_t pos) const
    {
        return (pos == 0) ? &(d.head) : tdata_at(d.tail(), pos - 1);
    }

 public:

    tuple_vals(tuple_vals const& other) : super(), m_data(other.m_data) { }

    tuple_vals() : m_data() { }

    tuple_vals(ElementTypes const&... args) : m_data(args...) { }

    inline data_type const& data() const { return m_data; }

    inline data_type& data_ref() { return m_data; }

    void* mutable_at(size_t pos)
    {
        return tdata_mutable_at(m_data, pos);
    }

    size_t size() const
    {
        return sizeof...(ElementTypes);
    }

    tuple_vals* copy() const
    {
        return new tuple_vals(*this);
    }

    void const* at(size_t pos) const
    {
        return tdata_at(m_data, pos);
    }

    uniform_type_info const* type_at(size_t pos) const
    {
        return m_types[pos];
    }

    bool equals(abstract_tuple const& other) const
    {
        if (size() != other.size()) return false;
        tuple_vals const* o = dynamic_cast<tuple_vals const*>(&other);
        if (o)
        {
            return m_data == (o->m_data);
        }
        return abstract_tuple::equals(other);
    }

};

template<typename... ElementTypes>
types_array<ElementTypes...> tuple_vals<ElementTypes...>::m_types;

} } // namespace cppa::detail

#endif // TUPLE_VALS_HPP
