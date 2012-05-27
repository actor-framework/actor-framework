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


#ifndef TYPE_VALUE_PAIR_HPP
#define TYPE_VALUE_PAIR_HPP

#include <cstddef>

#include "cppa/uniform_type_info.hpp"

namespace cppa {

typedef std::pair<uniform_type_info const*, void const*> type_value_pair;

class type_value_pair_const_iterator {

    type_value_pair const* iter;

 public:

    typedef type_value_pair const               value_type;
    typedef std::ptrdiff_t                      difference_type;
    typedef type_value_pair const*              pointer;
    typedef const type_value_pair&              reference;
    typedef std::bidirectional_iterator_tag     iterator_category;

    constexpr type_value_pair_const_iterator() : iter(nullptr) { }

    type_value_pair_const_iterator(type_value_pair const* i) : iter(i) { }

    type_value_pair_const_iterator(const type_value_pair_const_iterator&)
        = default;

    type_value_pair_const_iterator&
    operator=(const type_value_pair_const_iterator&) = default;

    inline uniform_type_info const* type() const { return iter->first; }

    inline void const* value() const { return iter->second; }

    inline decltype(iter) base() const { return iter; }

    inline decltype(iter) operator->() const { return iter; }

    inline decltype(*iter) operator*() const { return *iter; }

    inline type_value_pair_const_iterator& operator++() {
        ++iter;
        return *this;
    }

    inline type_value_pair_const_iterator operator++(int) {
        type_value_pair_const_iterator tmp{*this};
        ++iter;
        return tmp;
    }

    inline type_value_pair_const_iterator& operator--() {
        --iter;
        return *this;
    }

    inline type_value_pair_const_iterator operator--(int) {
        type_value_pair_const_iterator tmp{*this};
        --iter;
        return tmp;
    }

    inline type_value_pair_const_iterator operator+(size_t offset) {
        return iter + offset;
    }

    inline type_value_pair_const_iterator& operator+=(size_t offset) {
        iter += offset;
        return *this;
    }

};

/**
 * @relates type_value_pair_const_iterator
 */
inline bool operator==(const type_value_pair_const_iterator& lhs,
                       const type_value_pair_const_iterator& rhs) {
    return lhs.base() == rhs.base();
}

/**
 * @relates type_value_pair_const_iterator
 */
inline bool operator!=(const type_value_pair_const_iterator& lhs,
                       const type_value_pair_const_iterator& rhs) {
    return !(lhs == rhs);
}

} // namespace cppa

#endif // TYPE_VALUE_PAIR_HPP
