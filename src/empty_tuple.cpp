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


#include <stdexcept>
#include "cppa/detail/empty_tuple.hpp"

namespace cppa { namespace detail {

empty_tuple::empty_tuple() : abstract_tuple(tuple_impl_info::statically_typed) {
}

size_t empty_tuple::size() const {
    return 0;
}

void* empty_tuple::mutable_at(size_t) {
    throw std::range_error("empty_tuple::mutable_at()");
}

abstract_tuple* empty_tuple::copy() const {
    return new empty_tuple;
}

const void* empty_tuple::at(size_t) const {
    throw std::range_error("empty_tuple::at()");
}

const uniform_type_info* empty_tuple::type_at(size_t) const {
    throw std::range_error("empty_tuple::type_at()");
}

bool empty_tuple::equals(const abstract_tuple& other) const {
    return other.size() == 0;
}

const std::type_info* empty_tuple::type_token() const {
    return &typeid(util::type_list<>);
}

} } // namespace cppa::detail
