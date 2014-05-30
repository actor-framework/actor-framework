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


#ifndef CPPA_DETAIL_TUPLE_ITERATOR_HPP
#define CPPA_DETAIL_TUPLE_ITERATOR_HPP

#include <cstddef>

#include "cppa/config.hpp"

namespace cppa {
namespace detail {

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

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_TUPLE_ITERATOR_HPP
