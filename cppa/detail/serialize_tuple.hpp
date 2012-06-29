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


#ifndef CPPA_SERIALIZE_TUPLE_HPP
#define CPPA_SERIALIZE_TUPLE_HPP

#include <cstddef>

#include "cppa/uniform_type_info.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa { class serializer; }

namespace cppa { namespace detail {

template<typename List, size_t Pos = 0>
struct serialize_tuple {
    template<typename T>
    inline static void _(serializer& s, const T* tup) {
        s << uniform_typeid<typename List::head>()->name()
          << *reinterpret_cast<const typename List::head*>(tup->at(Pos));
        serialize_tuple<typename List::tail, Pos + 1>::_(s, tup);
    }
};

template<size_t Pos>
struct serialize_tuple<util::type_list<>, Pos> {
    template<typename T>
    inline static void _(serializer&, const T*) { }
};

} } // namespace cppa::detail

#endif // CPPA_SERIALIZE_TUPLE_HPP
