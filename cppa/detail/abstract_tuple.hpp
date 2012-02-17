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


#ifndef ABSTRACT_TUPLE_HPP
#define ABSTRACT_TUPLE_HPP

#include <iterator>

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/type_value_pair.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_list.hpp"

namespace cppa { namespace detail {

struct abstract_tuple : ref_counted
{

    // mutators
    virtual void* mutable_at(size_t pos) = 0;

    // accessors
    virtual size_t size() const = 0;
    virtual abstract_tuple* copy() const = 0;
    virtual void const* at(size_t pos) const = 0;
    virtual uniform_type_info const* type_at(size_t pos) const = 0;
    // type of the implementation class
    virtual std::type_info const& impl_type() const = 0;

    bool equals(abstract_tuple const& other) const;

    // iterator support
    class const_iterator : public std::iterator<std::bidirectional_iterator_tag,
                                                type_value_pair>
    {

        size_t m_pos;
        abstract_tuple const& m_tuple;

     public:

        const_iterator(abstract_tuple const& tup, size_t pos = 0) : m_pos(pos), m_tuple(tup) { }

        const_iterator(const_iterator const&) = default;

        inline bool operator==(const_iterator const& other) const
        {
            CPPA_REQUIRE(&(other.m_tuple) == &(other.m_tuple));
            return other.m_pos == m_pos;
        }

        inline bool operator!=(const_iterator const& other) const
        {
            return !(*this == other);
        }

        inline const_iterator& operator++()
        {
            ++m_pos;
            return *this;
        }

        inline const_iterator& operator--()
        {
            CPPA_REQUIRE(m_pos > 0);
            --m_pos;
            return *this;
        }

        inline const_iterator operator+(size_t offset)
        {
            return {m_tuple, m_pos + offset};
        }

        inline size_t position() const { return m_pos; }

        void const* value() const { return m_tuple.at(m_pos); }

        uniform_type_info const* type() const { return m_tuple.type_at(m_pos); }

        type_value_pair operator*() { return {type(), value()}; }

    };

    inline const_iterator begin() const { return {*this}; }

    inline const_iterator end() const { return {*this, size()}; }

};

} } // namespace cppa::detail

#endif // ABSTRACT_TUPLE_HPP
