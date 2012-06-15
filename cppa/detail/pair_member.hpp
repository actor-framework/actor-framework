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


#ifndef CPPA_PAIR_MEMBER_HPP
#define CPPA_PAIR_MEMBER_HPP

#include <utility>

#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/detail/type_to_ptype.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

namespace cppa { namespace detail {

template<typename T1, typename T2>
class pair_member : public util::abstract_uniform_type_info<std::pair<T1,T2>> {

    static constexpr primitive_type ptype1 = type_to_ptype<T1>::ptype;
    static constexpr primitive_type ptype2 = type_to_ptype<T2>::ptype;

    static_assert(ptype1 != pt_null, "T1 is not a primitive type");
    static_assert(ptype2 != pt_null, "T2 is not a primitive type");

    typedef std::pair<T1, T2> pair_type;

 public:

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

} } // namespace cppa::detail

#endif // CPPA_PAIR_MEMBER_HPP
