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


#ifndef ANY_TUPLE_VIEW_HPP
#define ANY_TUPLE_VIEW_HPP

#include <set>
#include <list>
#include <vector>
#include <utility>
#include <iterator>
#include <algorithm>

#include "cppa/tuple.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/enable_if.hpp"
#include "cppa/util/is_primitive.hpp"

#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"

namespace cppa {

class any_tuple_view
{

    typedef std::vector<type_value_pair> vector_type;

    vector_type m_values;

    template<typename T>
    void from_list(T const& list)
    {
        auto& arr = detail::static_types_array<typename T::value_type>::arr;
        for (auto i = list.begin(); i != list.end(); ++i)
        {
            m_values.push_back(std::make_pair(arr[0], &(*i)));
        }
    }

 public:

    class const_iterator;

    friend class const_iterator;

    any_tuple_view(any_tuple_view&&) = default;
    any_tuple_view(any_tuple_view const&) = default;
    any_tuple_view& operator=(any_tuple_view&&) = default;
    any_tuple_view& operator=(any_tuple_view const&) = default;

    any_tuple_view(any_tuple& tup)
    {
        if (tup.size() > 0)
        {
            // force tuple to detach
            tup.mutable_at(0);
            std::back_insert_iterator<vector_type> back_iter{m_values};
            std::copy(tup.begin(), tup.end(), back_iter);
        }
    }

    template<typename... T>
    any_tuple_view(tuple_view<T...> const& tup)
    {
        for (size_t i = 0; i < tup.size(); ++i)
        {
            m_values.push_back(std::make_pair(tup.type_at(i), tup.at(i)));
        }
    }

    template<typename F, typename S>
    any_tuple_view(std::pair<F, S> const& pair)
    {
        auto& arr = detail::static_types_array<F, S>::arr;
        m_values.push_back(std::make_pair(arr[0], &pair.first));
        m_values.push_back(std::make_pair(arr[1], &pair.second));
    }

    template<typename T>
    any_tuple_view(std::set<T>& vec) { from_list(vec); }

    template<typename T>
    any_tuple_view(std::list<T>& vec) { from_list(vec); }

    template<typename T>
    any_tuple_view(std::vector<T>& vec) { from_list(vec); }

    template<typename T>
    any_tuple_view(T& val,
                   typename util::enable_if<util::is_primitive<T> >::type* = 0)
    {
        auto& arr = detail::static_types_array<T>::arr;
        m_values.push_back(std::make_pair(arr[0], &val));
    }

    inline vector_type const& vals() const { return m_values; }

    inline size_t size() const { return m_values.size(); }

    inline void const* at(size_t p) const { return m_values[p].second; }

    void* mutable_at(size_t p) { return const_cast<void*>(m_values[p].second); }

    inline uniform_type_info const* type_at(size_t p) const
    {
        return m_values[p].first;
    }

    inline bool empty() const { return m_values.empty(); }

    template<typename T>
    inline T const& get_as(size_t p) const
    {
        return *reinterpret_cast<T const*>(at(p));
    }

    inline std::type_info const& impl_type() const
    {
        // this is a lie, but the pattern matching implementation
        // needs to do a full runtime check of each element
        return typeid(detail::object_array);
    }

};

} // namespace cppa

#endif // ANY_TUPLE_VIEW_HPP
