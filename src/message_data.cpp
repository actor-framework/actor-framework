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

#include "cppa/detail/message_data.hpp"

namespace cppa {
namespace detail {

message_data::message_data(bool is_dynamic) : m_is_dynamic(is_dynamic) {}

bool message_data::equals(const message_data& other) const {
    return this == &other ||
           (size() == other.size() &&
            std::equal(begin(), end(), other.begin(), detail::full_eq));
}

message_data::message_data(const message_data& other)
        : super(), m_is_dynamic(other.m_is_dynamic) {}

const std::type_info* message_data::type_token() const {
    return &typeid(void);
}

const void* message_data::native_data() const {
    return nullptr;
}

void* message_data::mutable_native_data() {
    return nullptr;
}

std::string get_tuple_type_names(const detail::message_data& tup) {
    std::string result = "@<>";
    for (size_t i = 0; i < tup.size(); ++i) {
        auto uti = tup.type_at(i);
        result += "+";
        result += uti->name();
    }
    return result;
}

message_data* message_data::ptr::get_detached() {
    auto p = m_ptr.get();
    if (!p->unique()) {
        auto np = p->copy();
        m_ptr.reset(np);
        return np;
    }
    return p;
}

} // namespace detail
} // namespace cppa
