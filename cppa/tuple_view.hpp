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


#ifndef TUPLE_VIEW_HPP
#define TUPLE_VIEW_HPP

#include <vector>
#include <cstring>

#include "cppa/get.hpp"
#include "cppa/tuple.hpp"
#include "cppa/any_tuple.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/util/compare_tuples.hpp"

#include "cppa/detail/decorated_tuple.hpp"

namespace cppa {

template<size_t N, typename... Types>
typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>& t);

/**
 * @brief Describes a view of an fixed-length tuple.
 */
template<typename... ElementTypes>
class tuple_view
{

    template<size_t N, typename... Types>
    friend typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>&);

    void const* m_data[sizeof...(ElementTypes)];

    tuple_view() { }

 public:

    static constexpr size_t num_elements = sizeof...(ElementTypes);

    typedef util::type_list<ElementTypes...> types;

    static tuple_view from(std::vector< std::pair<uniform_type_info const*, void const*> > const& vec)
    {
        tuple_view result;
        size_t j = 0;
        for (auto i = vec.begin(); i != vec.end(); ++i)
        {
            result.m_data[j++] = i->second;
        }
        return std::move(result);
    }

    static tuple_view from(std::vector< std::pair<uniform_type_info const*, void const*> > const& vec,
                           util::fixed_vector<size_t, sizeof...(ElementTypes)> const& mv)
    {
        tuple_view result;
        for (size_t i = 0; i < sizeof...(ElementTypes); ++i)
        {
            result.m_data[i] = vec[mv[i]].second;
        }
        return std::move(result);
    }

    tuple_view& operator=(tuple_view const& other)
    {
        memcpy(m_data, other.m_data, num_elements * sizeof(void*));
        return *this;
    }

    tuple_view(tuple_view const& other)
    {
        memcpy(m_data, other.m_data, num_elements * sizeof(void*));
    }

    inline size_t size() const
    {
        return sizeof...(ElementTypes);
    }

    inline void const* at(size_t p) const { return m_data[p]; }

};

template<size_t N, typename... Types>
const typename util::at<N, Types...>::type& get(tuple_view<Types...> const& t)
{
    static_assert(N < sizeof...(Types), "N >= t.size()");
    typedef typename util::at<N, Types...>::type result_t;
    return *reinterpret_cast<result_t const*>(t.at(N));
}

//template<size_t N, typename... Types>
//typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>& t)
//{
//    static_assert(N < sizeof...(Types), "N >= t.size()");
//    typedef typename util::at<N, Types...>::type result_t;
//    return *reinterpret_cast<result_t*>(t.m_vals->mutable_at(N));
//}

template<typename TypeList>
struct tuple_view_from_type_list;

template<typename... Types>
struct tuple_view_from_type_list<util::type_list<Types...>>
{
    typedef tuple_view<Types...> type;
};

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(tuple_view<LhsTypes...> const& lhs,
                       tuple_view<RhsTypes...> const& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(tuple<LhsTypes...> const& lhs,
                       tuple_view<RhsTypes...> const& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(tuple_view<LhsTypes...> const& lhs,
                       tuple<RhsTypes...> const& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(tuple_view<LhsTypes...> const& lhs,
                       tuple_view<RhsTypes...> const& rhs)
{
    return !(lhs == rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(tuple<LhsTypes...> const& lhs,
                       tuple_view<RhsTypes...> const& rhs)
{
    return !(lhs == rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(tuple_view<LhsTypes...> const& lhs,
                       tuple<RhsTypes...> const& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // TUPLE_VIEW_HPP
