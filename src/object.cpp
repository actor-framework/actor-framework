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


#include <algorithm>

#include "cppa/unit.hpp"
#include "cppa/config.hpp"
#include "cppa/object.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/detail/types_array.hpp"

namespace {

cppa::unit_t s_unit;

inline const cppa::uniform_type_info* unit_type() {
    return cppa::detail::static_types_array<cppa::unit_t>::arr[0];
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

object::object() : m_value(&s_unit), m_type(unit_type()) {
}

object::~object() {
    if (m_value != &s_unit) m_type->delete_instance(m_value);
}

object::object(const object& other) {
    m_type = other.m_type;
    m_value = (other.m_value == &s_unit) ? other.m_value
                                         : m_type->new_instance(other.m_value);
}

object::object(object&& other) : m_value(&s_unit), m_type(unit_type()) {
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
