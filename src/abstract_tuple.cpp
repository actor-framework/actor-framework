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


#include "cppa/detail/abstract_tuple.hpp"

namespace cppa {
namespace detail {

abstract_tuple::abstract_tuple(bool is_dynamic) : m_is_dynamic(is_dynamic) { }

bool abstract_tuple::equals(const abstract_tuple &other) const {
    return    this == &other
           || (   size() == other.size()
               && std::equal(begin(), end(), other.begin(), detail::full_eq));
}

abstract_tuple::abstract_tuple(const abstract_tuple& other)
: m_is_dynamic(other.m_is_dynamic) { }

const std::type_info* abstract_tuple::type_token() const {
    return &typeid(void);
}

const void* abstract_tuple::native_data() const {
    return nullptr;
}

void* abstract_tuple::mutable_native_data() {
    return nullptr;
}

std::string get_tuple_type_names(const detail::abstract_tuple& tup) {
    std::string result = "@<>";
    for (size_t i = 0; i < tup.size(); ++i) {
        auto uti = tup.type_at(i);
        result += "+";
        result += uti->name();
    }
    return result;
}

} // namespace detail
} // namespace cppa

