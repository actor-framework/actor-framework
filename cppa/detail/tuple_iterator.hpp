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


#ifndef CPPA_TUPLE_ITERATOR_HPP
#define CPPA_TUPLE_ITERATOR_HPP

#include <cstddef>

#include "cppa/config.hpp"

namespace cppa { namespace detail {

template<class Tuple>
class tuple_iterator {

    size_t m_pos;
    const Tuple* m_tuple;

 public:

    inline tuple_iterator(const Tuple* tup, size_t pos = 0)
        : m_pos(pos), m_tuple(tup) {
    }

    tuple_iterator(const tuple_iterator&) = default;

    tuple_iterator& operator=(const tuple_iterator&) = default;

    inline bool operator==(const tuple_iterator& other) const {
        CPPA_REQUIRE(other.m_tuple == other.m_tuple);
        return other.m_pos == m_pos;
    }

    inline bool operator!=(const tuple_iterator& other) const {
        return !(*this == other);
    }

    inline tuple_iterator& operator++() {
        ++m_pos;
        return *this;
    }

    inline tuple_iterator& operator--() {
        CPPA_REQUIRE(m_pos > 0);
        --m_pos;
        return *this;
    }

    inline tuple_iterator operator+(size_t offset) {
        return {m_tuple, m_pos + offset};
    }

    inline tuple_iterator& operator+=(size_t offset) {
        m_pos += offset;
        return *this;
    }

    inline tuple_iterator operator-(size_t offset) {
        CPPA_REQUIRE(m_pos >= offset);
        return {m_tuple, m_pos - offset};
    }

    inline tuple_iterator& operator-=(size_t offset) {
        CPPA_REQUIRE(m_pos >= offset);
        m_pos -= offset;
        return *this;
    }

    inline size_t position() const { return m_pos; }

    inline const void* value() const {
        return m_tuple->at(m_pos);
    }

    inline const uniform_type_info* type() const {
        return m_tuple->type_at(m_pos);
    }

    inline tuple_iterator& operator*() { return *this; }

};

} } // namespace cppa::detail

#endif // CPPA_TUPLE_ITERATOR_HPP
