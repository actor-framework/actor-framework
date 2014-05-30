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


#include "cppa/any_tuple.hpp"
#include "cppa/singletons.hpp"

namespace cppa {

any_tuple::any_tuple(detail::abstract_tuple* ptr) : m_vals(ptr) { }

any_tuple::any_tuple(any_tuple&& other) : m_vals(std::move(other.m_vals)) { }

any_tuple::any_tuple(const data_ptr& vals) : m_vals(vals) { }

any_tuple& any_tuple::operator=(any_tuple&& other) {
    m_vals.swap(other.m_vals);
    return *this;
}

void any_tuple::reset() {
    m_vals.reset();
}

void* any_tuple::mutable_at(size_t p) {
    CPPA_REQUIRE(m_vals != nullptr);
    return m_vals->mutable_at(p);
}

const void* any_tuple::at(size_t p) const {
    CPPA_REQUIRE(m_vals != nullptr);
    return m_vals->at(p);
}

const uniform_type_info* any_tuple::type_at(size_t p) const {
    CPPA_REQUIRE(m_vals != nullptr);
    return m_vals->type_at(p);
}

bool any_tuple::equals(const any_tuple& other) const {
    CPPA_REQUIRE(m_vals != nullptr);
    return m_vals->equals(*other.vals());
}

any_tuple any_tuple::drop(size_t n) const {
    CPPA_REQUIRE(m_vals != nullptr);
    if (n == 0) return *this;
    if (n >= size()) return any_tuple{};
    return any_tuple{detail::decorated_tuple::create(m_vals, n)};
}

any_tuple any_tuple::drop_right(size_t n) const {
    CPPA_REQUIRE(m_vals != nullptr);
    using namespace std;
    if (n == 0) return *this;
    if (n >= size()) return any_tuple{};
    vector<size_t> mapping(size() - n);
    size_t i = 0;
    generate(mapping.begin(), mapping.end(), [&] { return i++; });
    return any_tuple{detail::decorated_tuple::create(m_vals, move(mapping))};
}

} // namespace cppa
