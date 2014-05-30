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


#ifndef CPPA_DETAIL_SERIALIZE_TUPLE_HPP
#define CPPA_DETAIL_SERIALIZE_TUPLE_HPP

#include <cstddef>

#include "cppa/uniform_type_info.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa { class serializer; }

namespace cppa {
namespace detail {

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
struct serialize_tuple<util::empty_type_list, Pos> {
    template<typename T>
    inline static void _(serializer&, const T*) { }
};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_SERIALIZE_TUPLE_HPP
