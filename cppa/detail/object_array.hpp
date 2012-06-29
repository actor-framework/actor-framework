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


#ifndef CPPA_OBJECT_ARRAY_HPP
#define CPPA_OBJECT_ARRAY_HPP

#include <vector>

#include "cppa/object.hpp"
#include "cppa/detail/abstract_tuple.hpp"

namespace cppa { namespace detail {

class object_array : public abstract_tuple {

    typedef abstract_tuple super;

 public:

    using abstract_tuple::const_iterator;

    object_array();
    object_array(object_array&&) = default;
    object_array(const object_array&) = default;

    void push_back(object&& what);

    void push_back(const object& what);

    void* mutable_at(size_t pos);

    size_t size() const;

    abstract_tuple* copy() const;

    const void* at(size_t pos) const;

    bool equals(const cppa::detail::abstract_tuple&) const;

    const uniform_type_info* type_at(size_t pos) const;

 private:

    std::vector<object> m_elements;

};

} } // namespace cppa::detail

#endif // CPPA_OBJECT_ARRAY_HPP
