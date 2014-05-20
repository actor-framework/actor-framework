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
 * Copyright (C) 2011-2014                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_DETAIL_TUPLE_DUMMY_HPP
#define CPPA_DETAIL_TUPLE_DUMMY_HPP

#include <typeinfo>

#include "cppa/util/type_list.hpp"
#include "cppa/detail/tuple_iterator.hpp"

namespace cppa {
namespace detail {

struct tuple_dummy {
    typedef util::empty_type_list types;
    typedef detail::tuple_iterator<tuple_dummy> const_iterator;
    inline size_t size() const {
        return 0;
    }
    inline void* mutable_at(size_t) {
        return nullptr;
    }
    inline const void* at(size_t) const {
        return nullptr;
    }
    inline const uniform_type_info* type_at(size_t) const {
        return nullptr;
    }
    inline const std::type_info* type_token() const {
        return &typeid(util::empty_type_list);
    }
    inline bool dynamically_typed() const {
        return false;
    }
    inline const_iterator begin() const {
        return {this};
    }
    inline const_iterator end() const {
        return {this, 0};
    }
};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_TUPLE_DUMMY_HPP
