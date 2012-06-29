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


#ifndef CPPA_LIST_MEMBER_HPP
#define CPPA_LIST_MEMBER_HPP

#include "cppa/util/is_primitive.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

namespace cppa { namespace detail {

template<typename List, bool HasPrimitiveValues>
struct list_member_util {
    typedef typename List::value_type value_type;
    static constexpr primitive_type vptype = type_to_ptype<value_type>::ptype;
    void operator()(const List& list, serializer* s) const {
        s->begin_sequence(list.size());
        for (auto i = list.begin(); i != list.end(); ++i) {
            s->write_value(*i);
        }
        s->end_sequence();
    }
    void operator()(List& list, deserializer* d) const {
        list.clear();
        auto size = d->begin_sequence();
        for (decltype(size) i = 0; i < size; ++i) {
            list.push_back(get<value_type>(d->read_value(vptype)));
        }
        d->end_sequence();
    }
};

template<typename List>
struct list_member_util<List, false> {
    typedef typename List::value_type value_type;

    const uniform_type_info* m_value_type;

    list_member_util() : m_value_type(uniform_typeid<value_type>()) {
    }

    void operator()(const List& list, serializer* s) const {
        s->begin_sequence(list.size());
        for (auto i = list.begin(); i != list.end(); ++i) {
            m_value_type->serialize(&(*i), s);
        }
        s->end_sequence();
    }

    void operator()(List& list, deserializer* d) const {
        list.clear();
        value_type tmp;
        auto size = d->begin_sequence();
        for (decltype(size) i = 0; i < size; ++i) {
            m_value_type->deserialize(&tmp, d);
            list.push_back(tmp);
        }
        d->end_sequence();
    }
};

template<typename List>
class list_member : public util::abstract_uniform_type_info<List> {

    typedef typename List::value_type value_type;
    list_member_util<List, util::is_primitive<value_type>::value> m_helper;

 public:

    void serialize(const void* obj, serializer* s) const {
        auto& list = *reinterpret_cast<const List*>(obj);
        m_helper(list, s);
    }

    void deserialize(void* obj, deserializer* d) const {
        auto& list = *reinterpret_cast<List*>(obj);
        m_helper(list, d);
    }

};

} } // namespace cppa::detail

#endif // CPPA_LIST_MEMBER_HPP
