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

#include <list>
#include <vector>
#include <cstddef>
#include <type_traits>

#include "cppa/option.hpp"
#include "cppa/anything.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/type_value_pair.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/tbind.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/arg_match_t.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/static_foreach.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/tdata.hpp"
#include "cppa/detail/types_array.hpp"

namespace cppa {

template<class ExtendedType, class BasicType>
ExtendedType* extend_pattern(BasicType const* p);

template<typename... Types>
class pattern
{

    template<class ExtendedType, class BasicType>
    friend ExtendedType* extend_pattern(BasicType const* p);

    static_assert(sizeof...(Types) > 0, "empty pattern");

    pattern(pattern const&) = delete;
    pattern& operator=(pattern const&) = delete;

 public:

    static constexpr size_t size = sizeof...(Types);

    typedef util::type_list<Types...> types;

    typedef typename util::tl_filter_not<types, util::tbind<std::is_same, anything>::type>::type
            filtered_types;

    static constexpr size_t filtered_size = filtered_types::size;

    typedef util::fixed_vector<size_t, filtered_types::size> mapping_vector;

    typedef type_value_pair_const_iterator const_iterator;


    const_iterator begin() const { return m_ptrs; }

    const_iterator end() const { return m_ptrs + size; }

    pattern() : m_has_values(false)
    {
        auto& arr = detail::static_types_array<Types...>::arr;
        for (size_t i = 0; i < size; ++i)
        {
            m_ptrs[i].first = arr[i];
            m_ptrs[i].second = nullptr;
        }
    }

    template<typename Arg0, typename... Args>
    pattern(Arg0 const& arg0, Args const&... args) : m_data(arg0, args...)
    {
        using std::is_same;
        using namespace util;
        static constexpr bool all_boxed =
                util::tl_forall<type_list<Arg0, Args...>,
                                detail::is_boxed        >::value;
        m_has_values = !all_boxed;
        typedef typename type_list<Arg0, Args...>::back arg_n;
        static constexpr bool ignore_arg_n = is_same<arg_n, arg_match_t>::value;
        // ignore extra arg_match_t argument
        static constexpr size_t args_size = sizeof...(Args)
                                          + (ignore_arg_n ? 0 : 1);
        static_assert(args_size <= size, "too many arguments");
        auto& arr = detail::static_types_array<Types...>::arr;
        // "polymophic lambda"
        init_helper f(m_ptrs, arr);
        // init elements [0, N)
        util::static_foreach<0, args_size>::_(m_data, f);
        // init elements [N, size)
        for (size_t i = args_size; i < size; ++i)
        {
            m_ptrs[i].first = arr[i];
            m_ptrs[i].second = nullptr;
        }
    }

    inline bool has_values() const { return m_has_values; }

 private:

    struct init_helper
    {
        type_value_pair* iter_to;
        type_value_pair_const_iterator iter_from;
        init_helper(type_value_pair* pp, detail::types_array<Types...>& tarr)
            : iter_to(pp), iter_from(tarr.begin()) { }
        template<typename T>
        void operator()(option<T> const& what)
        {
            iter_to->first = iter_from.type();
            iter_to->second = (what) ? &(*what) : nullptr;
            ++iter_to;
            ++iter_from;
        }
    };

    detail::tdata<option<Types>...> m_data;
    bool m_has_values;
    type_value_pair m_ptrs[size];

};

template<class ExtendedType, class BasicType>
ExtendedType* extend_pattern(BasicType const* p)
{
    ExtendedType* et = new ExtendedType;
    detail::tdata_set(et->m_data, p->m_data);
    for (size_t i = 0; i < BasicType::size; ++i)
    {
        et->m_ptrs[i].first = p->m_ptrs[i].first;
        if (p->m_ptrs[i].second != nullptr)
        {
            et->m_ptrs[i].second = et->m_data.at(i);
        }
    }
    typedef typename ExtendedType::types extended_types;
    typedef typename detail::static_types_array_from_type_list<extended_types>::type tarr;
    auto& arr = tarr::arr;
    for (auto i = BasicType::size; i < ExtendedType::size; ++i)
    {
        et->m_ptrs[i].first = arr[i];
    }
    return et;
}

template<typename TypeList>
struct pattern_from_type_list;

template<typename... Types>
struct pattern_from_type_list<util::type_list<Types...>>
{
    typedef pattern<Types...> type;
};

} // namespace cppa

#endif // PATTERN_HPP
