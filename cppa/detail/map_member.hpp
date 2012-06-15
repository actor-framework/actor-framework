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


#ifndef CPPA_MAP_MEMBER_HPP
#define CPPA_MAP_MEMBER_HPP

#include <type_traits>
#include "cppa/detail/pair_member.hpp"
#include "cppa/detail/primitive_member.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

namespace cppa { namespace detail {

template<typename T>
struct is_pair { static constexpr bool value = false; };

template<typename First, typename Second>
struct is_pair< std::pair<First, Second> > { static constexpr bool value = true; };

template<typename T, bool IsPair, bool IsPrimitive>
struct map_member_util;

// std::set with primitive value
template<typename T>
struct map_member_util<T, false, true> {
    primitive_member<T> impl;

    void serialize_value(const T& what, serializer* s) const {
        impl.serialize(&what, s);
    }

    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const {
        T value;
        impl.deserialize(&value, d);
        map.insert(std::move(value));
    }
};

// std::set with non-primitive value
template<typename T>
struct map_member_util<T, false, false> {
    const uniform_type_info* m_type;

    map_member_util() : m_type(uniform_typeid<T>()) {
    }

    void serialize_value(const T& what, serializer* s) const {
        m_type->serialize(&what, s);
    }

    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const {
        T value;
        m_type->deserialize(&value, d);
        map.insert(std::move(value));
    }
};

// std::map
template<typename T>
struct map_member_util<T, true, false> {

    // std::map defines its first half of value_type as const
    typedef typename std::remove_const<typename T::first_type>::type first_type;
    typedef typename T::second_type second_type;

    pair_member<first_type, second_type> impl;


    void serialize_value(const T& what, serializer* s) const {
        // impl needs a pair without const modifier
        std::pair<first_type, second_type> p(what.first, what.second);
        impl.serialize(&p, s);
    }

    template<typename M>
    void deserialize_and_insert(M& map, deserializer* d) const {
        std::pair<first_type, second_type> p;
        impl.deserialize(&p, d);
        std::pair<const first_type, second_type> v(std::move(p.first),
                                                   std::move(p.second));
        map.insert(std::move(v));
    }

};

template<typename Map>
class map_member : public util::abstract_uniform_type_info<Map> {

    typedef typename Map::key_type key_type;
    typedef typename Map::value_type value_type;

    map_member_util<value_type,
                    is_pair<value_type>::value,
                    util::is_primitive<value_type>::value> m_helper;

 public:

    void serialize(const void* obj, serializer* s) const {
        auto& mp = *reinterpret_cast<const Map*>(obj);
        s->begin_sequence(mp.size());
        for (const auto& val : mp) {
            m_helper.serialize_value(val, s);
        }
        s->end_sequence();
    }

    void deserialize(void* obj, deserializer* d) const {
        auto& mp = *reinterpret_cast<Map*>(obj);
        mp.clear();
        size_t mp_size = d->begin_sequence();
        for (size_t i = 0; i < mp_size; ++i) {
            m_helper.deserialize_and_insert(mp, d);
        }
        d->end_sequence();
    }

};

} } // namespace cppa::detail

#endif // CPPA_MAP_MEMBER_HPP
