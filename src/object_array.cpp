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


#include "cppa/detail/object_array.hpp"

namespace cppa {
namespace detail {

object_array::object_array() : super(true) {
    // nop
}

object_array::object_array(const object_array& other) : super(true) {
    m_elements.reserve(other.m_elements.size());
    for (auto& e : other.m_elements) m_elements.push_back(e->copy());
}

object_array::~object_array() {
    // nop
}

void object_array::push_back(uniform_value what) {
    CPPA_REQUIRE(what && what->val != nullptr && what->ti != nullptr);
    m_elements.push_back(std::move(what));
}

void* object_array::mutable_at(size_t pos) {
    CPPA_REQUIRE(pos < size());
    return m_elements[pos]->val;
}

size_t object_array::size() const {
    return m_elements.size();
}

object_array* object_array::copy() const {
    return new object_array(*this);
}

const void* object_array::at(size_t pos) const {
    CPPA_REQUIRE(pos < size());
    return m_elements[pos]->val;
}

const uniform_type_info* object_array::type_at(size_t pos) const {
    CPPA_REQUIRE(pos < size());
    return m_elements[pos]->ti;
}

const std::string* object_array::tuple_type_names() const {
    return nullptr; // get_tuple_type_names(*this);
}

} // namespace util
} // namespace cppa

