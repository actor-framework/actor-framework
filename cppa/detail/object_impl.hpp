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


#ifndef CPPA_OBJECT_IMPL_HPP
#define CPPA_OBJECT_IMPL_HPP

#include "cppa/object.hpp"

namespace cppa {

template<typename T>
const utype& uniform_type_info();

}

namespace cppa { namespace detail {

template<typename T>
struct obj_impl : object {
    T m_value;
    obj_impl() : m_value() { }
    obj_impl(const T& v) : m_value(v) { }
    virtual object* copy() const { return new obj_impl(m_value); }
    virtual const utype& type() const { return uniform_type_info<T>(); }
    virtual void* mutable_value() { return &m_value; }
    virtual const void* value() const { return &m_value; }
    virtual void serialize(serializer& s) const {
        s << m_value;
    }
    virtual void deserialize(deserializer& d) {
        d >> m_value;
    }
};

} } // namespace cppa::detail

#endif // CPPA_OBJECT_IMPL_HPP
