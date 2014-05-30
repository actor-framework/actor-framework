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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_DETAIL_PAIR_MEMBER_HPP
#define CPPA_DETAIL_PAIR_MEMBER_HPP

#include <utility>

#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/primitive_variant.hpp"

#include "cppa/util/is_builtin.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

#include "cppa/detail/types_array.hpp"
#include "cppa/detail/type_to_ptype.hpp"

namespace cppa {
namespace detail {

template<typename T1, typename T2, bool PrimitiveTypes> // default: true
struct pair_member_impl {

    static constexpr primitive_type ptype1 = type_to_ptype<T1>::ptype;
    static constexpr primitive_type ptype2 = type_to_ptype<T2>::ptype;

    typedef std::pair<T1, T2> pair_type;

    void serialize(const void* obj, serializer* s) const {
        auto& p = *reinterpret_cast<const pair_type*>(obj);
        primitive_variant values[2] = { p.first, p.second };
        s->write_tuple(2, values);
    }

    void deserialize(void* obj, deserializer* d) const {
        primitive_type ptypes[2] = { ptype1, ptype2 };
        primitive_variant values[2];
        d->read_tuple(2, ptypes, values);
        auto& p = *reinterpret_cast<pair_type*>(obj);
        p.first = std::move(get<T1>(values[0]));
        p.second = std::move(get<T2>(values[1]));
    }

};

template<typename T1, typename T2>
struct pair_member_impl<T1, T2, false> {

    typedef std::pair<T1, T2> pair_type;

    void serialize(const void* obj, serializer* s) const {
        auto& arr = static_types_array<T1, T2>::arr;
        auto& p = *reinterpret_cast<const pair_type*>(obj);
        arr[0]->serialize(&p.first, s);
        arr[1]->serialize(&p.second, s);
    }

    void deserialize(void* obj, deserializer* d) const {
        auto& arr = static_types_array<T1, T2>::arr;
        auto& p = *reinterpret_cast<pair_type*>(obj);
        arr[0]->deserialize(&p.first, d);
        arr[1]->deserialize(&p.second, d);
    }

};

template<typename T1, typename T2>
class pair_member : public util::abstract_uniform_type_info<std::pair<T1, T2>> {

    static_assert(util::is_builtin<T1>::value, "T1 is not a builtin type");
    static_assert(util::is_builtin<T1>::value, "T2 is not a builtin type");

 public:

    void serialize(const void* obj, serializer* s) const {
        impl.serialize(obj, s);
    }

    void deserialize(void* obj, deserializer* d) const {
        impl.deserialize(obj, d);
    }

 private:

    pair_member_impl<T1, T2,    util::is_primitive<T1>::value
                             && util::is_primitive<T2>::value> impl;


};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_PAIR_MEMBER_HPP
