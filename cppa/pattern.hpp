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


#ifndef LIBCPPA_PATTERN_HPP
#define LIBCPPA_PATTERN_HPP

#include <vector>
#include <cstddef>
#include <type_traits>

#include "cppa/anything.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_list.hpp"
#include "cppa/util/type_list_apply.hpp"

#include "cppa/detail/tdata.hpp"
#include "cppa/detail/pattern_details.hpp"

namespace cppa {

template<typename... Types>
class pattern;

template<typename T0, typename... Tn>
class pattern<T0, Tn...>
{

    pattern(pattern const&) = delete;
    pattern& operator=(pattern const&) = delete;

 public:

    static constexpr size_t size = sizeof...(Tn) + 1;

    typedef util::type_list<T0, Tn...> tpl_args;

    typedef typename util::filter_type_list<anything, tpl_args>::type
            filtered_tpl_args;

    typedef typename tuple_view_type_from_type_list<filtered_tpl_args>::type
            tuple_view_type;

    typedef typename tuple_view_type::mapping_vector mapping_vector;

    pattern()
    {
        cppa::uniform_type_info const* * iter = m_utis;
        detail::fill_uti_vec<decltype(iter), T0, Tn...>(iter);
        for (size_t i = 0; i < size; ++i)
        {
            m_data_ptr[i] = nullptr;
        }
    }

    template<typename Arg0, typename... Args>
    pattern(Arg0 const& arg0, Args const&... args) : m_data(arg0, args...)
    {
        bool invalid_args[] = { detail::is_boxed<Arg0>::value,
                                detail::is_boxed<Args>::value... };
        detail::fill_vecs<decltype(m_data), T0, Tn...>(0,
                                                       sizeof...(Args) + 1,
                                                       invalid_args,
                                                       m_data,
                                                       m_utis, m_data_ptr);
    }

    bool operator()(cppa::any_tuple const& msg,
                    mapping_vector* mapping = nullptr) const
    {
        detail::pattern_iterator arg0(size, m_data_ptr, m_utis);
        detail::tuple_iterator_arg<mapping_vector> arg1(msg, mapping);
        return detail::do_match(arg0, arg1);
    }

 private:

    detail::tdata<T0, Tn...> m_data;
    cppa::uniform_type_info const* m_utis[size];
    void const* m_data_ptr[size];

};

template<typename TypeList>
struct pattern_type_from_type_list;

template<typename... Types>
struct pattern_type_from_type_list<util::type_list<Types...>>
{
    typedef pattern<Types...> type;
};

} // namespace cppa

#endif // PATTERN_HPP
