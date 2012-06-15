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


#ifndef CPPA_PRIMITIVE_MEMBER_HPP
#define CPPA_PRIMITIVE_MEMBER_HPP

#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/detail/type_to_ptype.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

namespace cppa { namespace detail {

// uniform_type_info implementation for primitive data types.
template<typename T>
class primitive_member : public util::abstract_uniform_type_info<T> {

    static constexpr primitive_type ptype = type_to_ptype<T>::ptype;

    static_assert(ptype != pt_null, "T is not a primitive type");

 public:

    void serialize(const void* obj, serializer* s) const {
        s->write_value(*reinterpret_cast<const T*>(obj));
    }

    void deserialize(void* obj, deserializer* d) const {
        primitive_variant val(d->read_value(ptype));
        *reinterpret_cast<T*>(obj) = std::move(get<T>(val));
    }

};

} } // namespace cppa::detail

#endif // CPPA_PRIMITIVE_MEMBER_HPP
