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


#ifndef CPPA_ABSTRACT_TUPLE_HPP
#define CPPA_ABSTRACT_TUPLE_HPP

#include <iterator>
#include <typeinfo>

#include "cppa/config.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_list.hpp"

#include "cppa/detail/tuple_iterator.hpp"

namespace cppa { namespace detail {

enum tuple_impl_info {
    statically_typed,
    dynamically_typed
};

class abstract_tuple : public ref_counted {

    tuple_impl_info m_impl_type;

 public:

    inline abstract_tuple(tuple_impl_info tii) : m_impl_type(tii) { }
    abstract_tuple(const abstract_tuple& other);

    // mutators
    virtual void* mutable_at(size_t pos) = 0;
    virtual void* mutable_native_data();

    // accessors
    virtual size_t size() const = 0;
    virtual abstract_tuple* copy() const = 0;
    virtual const void* at(size_t pos) const = 0;
    virtual const uniform_type_info* type_at(size_t pos) const = 0;

    // returns either tdata<...> object or nullptr (default) if tuple
    // is not a 'native' implementation
    virtual const void* native_data() const;

    // Identifies the type of the implementation.
    // A statically typed tuple implementation can use some optimizations,
    // e.g., "impl_type() == statically_typed" implies that type_token()
    // identifies all possible instances of a given tuple implementation
    inline tuple_impl_info impl_type() const { return m_impl_type; }

    // uniquely identifies this category (element types) of messages
    // override this member function only if impl_type() == statically_typed
    // (default returns &typeid(void))
    virtual const std::type_info* type_token() const;

    bool equals(const abstract_tuple& other) const;

    typedef tuple_iterator<abstract_tuple> const_iterator;

    inline const_iterator  begin() const { return {this}; }
    inline const_iterator cbegin() const { return {this}; }

    inline const_iterator  end() const { return {this, size()}; }
    inline const_iterator cend() const { return {this, size()}; }

};

struct full_eq_type {
    constexpr full_eq_type() { }
    template<class Tuple>
    inline bool operator()(const tuple_iterator<Tuple>& lhs,
                           const tuple_iterator<Tuple>& rhs) const {
        return    lhs.type() == rhs.type()
               && lhs.type()->equals(lhs.value(), rhs.value());
    }
};

struct types_only_eq_type {
    constexpr types_only_eq_type() { }
    template<class Tuple>
    inline bool operator()(const tuple_iterator<Tuple>& lhs,
                           const uniform_type_info* rhs     ) const {
        return lhs.type() == rhs;
    }
    template<class Tuple>
    inline bool operator()(const uniform_type_info* lhs,
                           const tuple_iterator<Tuple>& rhs) const {
        return lhs == rhs.type();
    }
};

namespace {
constexpr full_eq_type full_eq;
constexpr types_only_eq_type types_only_eq;
} // namespace <anonymous>

} } // namespace cppa::detail

#endif // CPPA_ABSTRACT_TUPLE_HPP
