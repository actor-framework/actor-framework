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


#include <algorithm>

#include "cppa/config.hpp"
#include "cppa/object.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/void_type.hpp"
#include "cppa/detail/types_array.hpp"

namespace {

cppa::util::void_type s_void;

inline const cppa::uniform_type_info* tvoid() {
    return cppa::detail::static_types_array<cppa::util::void_type>::arr[0];
}

} // namespace <anonymous>

namespace cppa {

void object::swap(object& other) {
    std::swap(m_value, other.m_value);
    std::swap(m_type, other.m_type);
}

object::object(void* val, const uniform_type_info* utype)
    : m_value(val), m_type(utype) {
    CPPA_REQUIRE(val != nullptr);
    CPPA_REQUIRE(utype != nullptr);
}

object::object() : m_value(&s_void), m_type(tvoid()) {
}

object::~object() {
    if (m_value != &s_void) m_type->delete_instance(m_value);
}

object::object(const object& other) {
    m_type = other.m_type;
    m_value = (other.m_value == &s_void) ? other.m_value
                                         : m_type->new_instance(other.m_value);
}

object::object(object&& other) : m_value(&s_void), m_type(tvoid()) {
    swap(other);
}

object& object::operator=(object&& other) {
    object tmp(std::move(other));
    swap(tmp);
    return *this;
}

object& object::operator=(const object& other) {
    object tmp(other);
    swap(tmp);
    return *this;
}

bool operator==(const object& lhs, const object& rhs) {
    if (lhs.type() == rhs.type()) {
        // values might both point to s_void if lhs and rhs are "empty"
        return    lhs.value() == rhs.value()
               || lhs.type()->equals(lhs.value(), rhs.value());
    }
    return false;
}

const uniform_type_info* object::type() const {
    return m_type;
}

const void* object::value() const {
    return m_value;
}

void* object::mutable_value() {
    return m_value;
}

} // namespace cppa
