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

object_array::object_array() : super(true) { }

void object_array::push_back(const object& what) {
    m_elements.push_back(what);
}

void object_array::push_back(object&& what) {
    m_elements.push_back(std::move(what));
}

void* object_array::mutable_at(size_t pos) {
    return m_elements[pos].mutable_value();
}

size_t object_array::size() const {
    return m_elements.size();
}

object_array* object_array::copy() const {
    return new object_array(*this);
}

const void* object_array::at(size_t pos) const {
    return m_elements[pos].value();
}

const uniform_type_info* object_array::type_at(size_t pos) const {
    return m_elements[pos].type();
}

const std::string* object_array::tuple_type_names() const {
    return nullptr; // get_tuple_type_names(*this);
}

} // namespace util
} // namespace cppa

