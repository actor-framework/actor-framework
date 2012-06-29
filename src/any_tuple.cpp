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


#include "cppa/any_tuple.hpp"
#include "cppa/detail/empty_tuple.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

namespace {
inline detail::empty_tuple* s_empty_tuple() {
    return detail::singleton_manager::get_empty_tuple();
}
} // namespace <anonymous>

any_tuple::any_tuple() : m_vals(s_empty_tuple()) { }

any_tuple::any_tuple(detail::abstract_tuple* ptr) : m_vals(ptr) { }

any_tuple::any_tuple(any_tuple&& other) : m_vals(s_empty_tuple()) {
    m_vals.swap(other.m_vals);
}

any_tuple::any_tuple(const value_ptr& vals) : m_vals(vals) { }

any_tuple& any_tuple::operator=(any_tuple&& other) {
    m_vals.swap(other.m_vals);
    return *this;
}

void any_tuple::reset() {
    m_vals.reset(s_empty_tuple());
}

size_t any_tuple::size() const {
    return m_vals->size();
}

void* any_tuple::mutable_at(size_t p) {
    return m_vals->mutable_at(p);
}

const void* any_tuple::at(size_t p) const {
    return m_vals->at(p);
}

const uniform_type_info* any_tuple::type_at(size_t p) const {
    return m_vals->type_at(p);
}

bool any_tuple::equals(const any_tuple& other) const {
    return m_vals->equals(*other.vals());
}

} // namespace cppa
