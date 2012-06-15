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


#ifndef CPPA_CONTAINER_TUPLE_VIEW_HPP
#define CPPA_CONTAINER_TUPLE_VIEW_HPP

#include <iostream>

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/disablable_delete.hpp"

namespace cppa { namespace detail {

template<class Container>
class container_tuple_view : public abstract_tuple {

    typedef abstract_tuple super;

 public:

    typedef typename Container::value_type value_type;

    container_tuple_view(Container* c, bool take_ownership = false)
        : super(tuple_impl_info::dynamically_typed), m_ptr(c) {
        CPPA_REQUIRE(c != nullptr);
        if (!take_ownership) m_ptr.get_deleter().disable();
    }

    size_t size() const {
        return m_ptr->size();
    }

    abstract_tuple* copy() const {
        return new container_tuple_view{new Container(*m_ptr), true};
    }

    const void* at(size_t pos) const {
        CPPA_REQUIRE(pos < size());
        auto i = m_ptr->begin();
        std::advance(i, pos);
        return &(*i);
    }

    void* mutable_at(size_t pos) {
        CPPA_REQUIRE(pos < size());
        auto i = m_ptr->begin();
        std::advance(i, pos);
        return &(*i);
    }

    const uniform_type_info* type_at(size_t) const {
        return static_types_array<value_type>::arr[0];
    }

 private:

    std::unique_ptr<Container, disablable_delete<Container> > m_ptr;

};

} } // namespace cppa::detail

#endif // CPPA_CONTAINER_TUPLE_VIEW_HPP
