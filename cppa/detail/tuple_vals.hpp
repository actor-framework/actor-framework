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


#ifndef CPPA_TUPLE_VALS_HPP
#define CPPA_TUPLE_VALS_HPP

#include <stdexcept>

#include "cppa/util/type_list.hpp"

#include "cppa/detail/tdata.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/serialize_tuple.hpp"

namespace cppa { namespace detail {

template<typename... ElementTypes>
class tuple_vals : public abstract_tuple {

    static_assert(sizeof...(ElementTypes) > 0,
                  "tuple_vals is not allowed to be empty");

    typedef abstract_tuple super;

 public:

    typedef tdata<ElementTypes...> data_type;

    typedef types_array<ElementTypes...> element_types;

    tuple_vals() : super(tuple_impl_info::statically_typed), m_data() { }

    tuple_vals(const tuple_vals&) = default;

    tuple_vals(const ElementTypes&... args)
        : super(tuple_impl_info::statically_typed), m_data(args...) {
    }

    const void* native_data() const {
        return &m_data;
    }

    void* mutable_native_data() {
        return &m_data;
    }

    inline data_type& data() {
        return m_data;
    }

    inline const data_type& data() const {
        return m_data;
    }

    size_t size() const {
        return sizeof...(ElementTypes);
    }

    tuple_vals* copy() const {
        return new tuple_vals(*this);
    }

    const void* at(size_t pos) const {
        CPPA_REQUIRE(pos < size());
        return m_data.at(pos);
    }

    void* mutable_at(size_t pos) {
        CPPA_REQUIRE(pos < size());
        return const_cast<void*>(at(pos));
    }

    const uniform_type_info* type_at(size_t pos) const {
        CPPA_REQUIRE(pos < size());
        return m_types[pos];
    }

    bool equals(const abstract_tuple& other) const {
        if (size() != other.size()) return false;
        const tuple_vals* o = dynamic_cast<const tuple_vals*>(&other);
        if (o) {
            return m_data == (o->m_data);
        }
        return abstract_tuple::equals(other);
    }

    const std::type_info* type_token() const {
        return detail::static_type_list<ElementTypes...>::list;
    }

 private:

    data_type m_data;

    static types_array<ElementTypes...> m_types;

};

template<typename... ElementTypes>
types_array<ElementTypes...> tuple_vals<ElementTypes...>::m_types;

template<typename TypeList>
struct tuple_vals_from_type_list;

template<typename... Types>
struct tuple_vals_from_type_list< util::type_list<Types...> > {
    typedef tuple_vals<Types...> type;
};

} } // namespace cppa::detail

#endif // CPPA_TUPLE_VALS_HPP
