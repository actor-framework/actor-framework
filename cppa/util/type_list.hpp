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


#ifndef LIBCPPA_UTIL_TYPE_LIST_HPP
#define LIBCPPA_UTIL_TYPE_LIST_HPP

#include <typeinfo>

#include "cppa/util/if_else.hpp"
#include "cppa/util/void_type.hpp"

namespace cppa {

// forward declarations
class uniform_type_info;
uniform_type_info const* uniform_typeid(std::type_info const&);

} // namespace cppa

namespace cppa { namespace util {

template<typename... Types> struct type_list;

template<>
struct type_list<>
{
    typedef void_type head_type;
    typedef void_type back_type;
    typedef type_list<> tail_type;
    static const size_t size = 0;
    template<typename, int = 0>
    struct find
    {
        static const int value = -1;
    };
};

template<typename Head, typename... Tail>
struct type_list<Head, Tail...>
{

    typedef uniform_type_info const* uti_ptr;

    typedef Head head_type;

    typedef type_list<Tail...> tail_type;

    typedef typename if_else_c<sizeof...(Tail) == 0,
                               Head,
                               wrapped<typename tail_type::back_type> >::type
            back_type;

    static const size_t size =  sizeof...(Tail) + 1;

    type_list()
    {
        init<type_list>(m_arr);
    }

    inline uniform_type_info const& at(size_t pos) const
    {
        return *m_arr[pos];
    }

    template<typename TypeList>
    static void init(uti_ptr* what)
    {
        what[0] = uniform_typeid(typeid(typename TypeList::head_type));
        if (TypeList::size > 1)
        {
            ++what;
            init<typename TypeList::tail_type>(what);
        }
    }

    template<typename What, int Pos = 0>
    struct find
    {
        static const int value = std::is_same<What,Head>::value
                                 ? Pos
                                 : type_list<Tail...>::template find<What,Pos+1>::value;
    };

 private:

    uti_ptr m_arr[size];

};

} } // namespace cppa::util

#endif // LIBCPPA_UTIL_TYPE_LIST_HPP
