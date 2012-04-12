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
#include <typeinfo>

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_list.hpp"

namespace cppa { namespace detail {

enum tuple_impl_info
{
    statically_typed,
    dynamically_typed
};

class abstract_tuple : public ref_counted
{

    tuple_impl_info m_impl_type;

 public:

    inline abstract_tuple(tuple_impl_info tii) : m_impl_type(tii) { }
    abstract_tuple(abstract_tuple const& other);

    // mutators
    virtual void* mutable_at(size_t pos) = 0;
    virtual void* mutable_native_data();

    // accessors
    virtual size_t size() const = 0;
    virtual abstract_tuple* copy() const = 0;
    virtual void const* at(size_t pos) const = 0;
    virtual uniform_type_info const* type_at(size_t pos) const = 0;

    // returns either tdata<...> object or nullptr (default) if tuple
    // is not a 'native' implementation
    virtual void const* native_data() const;

    // Identifies the type of the implementation.
    // A statically typed tuple implementation can use some optimizations,
    // e.g., "impl_type() == statically_typed" implies that type_token()
    // identifies all possible instances of a given tuple implementation
    inline tuple_impl_info impl_type() const { return m_impl_type; }

    // uniquely identifies this category (element types) of messages
    // override this member function only if impl_type() == statically_typed
    // (default returns &typeid(void))
    virtual std::type_info const* type_token() const;

    bool equals(abstract_tuple const& other) const;

    // iterator support
    class const_iterator
    {

        size_t m_pos;
        abstract_tuple const* m_tuple;

     public:

        inline const_iterator(abstract_tuple const* tup, size_t pos = 0)
            : m_pos(pos), m_tuple(tup)
        {
        }

        const_iterator(const_iterator const&) = default;

        const_iterator& operator=(const_iterator const&) = default;

        inline bool operator==(const_iterator const& other) const
        {
            CPPA_REQUIRE(other.m_tuple == other.m_tuple);
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

        inline const_iterator& operator+=(size_t offset)
        {
            m_pos += offset;
            return *this;
        }

        inline const_iterator operator-(size_t offset)
        {
            CPPA_REQUIRE(m_pos >= offset);
            return {m_tuple, m_pos - offset};
        }

        inline const_iterator& operator-=(size_t offset)
        {
            CPPA_REQUIRE(m_pos >= offset);
            m_pos -= offset;
            return *this;
        }

        inline size_t position() const { return m_pos; }

        inline void const* value() const
        {
            return m_tuple->at(m_pos);
        }

        inline uniform_type_info const* type() const
        {
            return m_tuple->type_at(m_pos);
        }

        inline const_iterator& operator*() { return *this; }

    };

    inline const_iterator begin() const { return {this}; }
    inline const_iterator cbegin() const { return {this}; }

    inline const_iterator end() const { return {this, size()}; }
    inline const_iterator cend() const { return {this, size()}; }

};

inline bool full_eq_v3(abstract_tuple::const_iterator const& lhs,
                       abstract_tuple::const_iterator const& rhs)
{
    return    lhs.type() == rhs.type()
           && lhs.type()->equals(lhs.value(), rhs.value());
}

inline bool types_only_eq(abstract_tuple::const_iterator const& lhs,
                          uniform_type_info const* rhs)
{
    return lhs.type() == rhs;
}

inline bool types_only_eq_v2(uniform_type_info const* lhs,
                             abstract_tuple::const_iterator const& rhs)
{
    return lhs == rhs.type();
}

} } // namespace cppa::detail

#endif // ABSTRACT_TUPLE_HPP
