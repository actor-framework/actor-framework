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


#ifndef TUPLE_CAST_IMPL_HPP
#define TUPLE_CAST_IMPL_HPP

#include "cppa/pattern.hpp"
#include "cppa/type_value_pair.hpp"

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/decorated_tuple.hpp"

namespace cppa { namespace detail {

enum class tuple_cast_impl_id
{
    no_wildcard,
    trailing_wildcard,
    leading_wildcard,
    wildcard_in_between
};

template<tuple_cast_impl_id, class Result, typename... T>
struct tuple_cast_impl;

template<class Result, typename... T>
struct tuple_cast_impl<tuple_cast_impl_id::no_wildcard, Result, T...>
{
    template<class Tuple>
    inline static bool types_match(Tuple const& tup)
    {
        auto& impl_typeid = tup.impl_type();
        if (   impl_typeid == typeid(tuple_vals<T...>)
            || impl_typeid == typeid(decorated_tuple<T...>))
        {
            return true;
        }
        else if (impl_typeid == typeid(object_array))
        {
            if (tup.size() == sizeof...(T))
            {
                auto& tarr = static_types_array<T...>::arr;
                for (size_t i = 0; i < sizeof...(T); ++i)
                {
                    if (tarr[i] != tup.type_at(i)) return false;
                }
                return true;
            }
        }
        return false;
    }
    template<class Tuple>
    static option<Result> _(Tuple const& tup, pattern<T...> const& p)
    {
        if (types_match(tup))
        {
            if (   p.has_values() == false
                || std::equal(tup.begin(), tup.end(), p.begin(),
                              values_only_eq))
            {
                return {Result::from(tup.vals())};
            }
        }
        return {};
    }
    template<class Tuple>
    static option<Result> _(Tuple const& tup)
    {
        if (types_match(tup))
        {
            return {Result::from(tup.vals())};
        }
        return {};
    }
};

template<class Result, typename... T>
struct tuple_cast_impl<tuple_cast_impl_id::trailing_wildcard, Result, T...>
{
    static_assert(sizeof...(T) > 1, "tuple_cast<anything> (empty result)");
    template<class Tuple>
    inline static option<Result> _(Tuple const& tup, pattern<T...> const& p)
    {
        if (tup.size() >= (sizeof...(T) - 1))
        {
            // skip last element (wildcard)
            auto end = tup.end();
            --end;
            if (std::equal(tup.begin(), end, p.begin(),
                           (p.has_values() ? full_eq
                                           : types_only_eq)))
            {
                return {Result::subtuple(tup.vals())};
            }
        }
        return {};
    }
    template<class Tuple>
    static option<Result> _(Tuple const& tup)
    {
        if (tup.size() >= (sizeof...(T) - 1))
        {
            auto& tarr = static_types_array<T...>::arr;
            for (size_t i = 0; i < (sizeof...(T) - 1); ++i)
            {
                if (tarr[i] != tup.type_at(i)) return { };
            }
            return {Result::subtuple(tup.vals())};
        }
        return {};
    }
};

template<class Result, typename... T>
struct tuple_cast_impl<tuple_cast_impl_id::leading_wildcard, Result, T...>
{
    static_assert(sizeof...(T) > 1, "tuple_cast<anything> (empty result)");
    template<class Tuple>
    inline static option<Result> _(Tuple const& tup, pattern<T...> const& p)
    {
        if (tup.size() >= (sizeof...(T) - 1))
        {
            size_t offset = tup.size() - (sizeof...(T) - 1);
            // skip first elements in tuple
            auto begin = tup.begin();
            begin += offset;
            // skip first element (wildcard) of pattern
            auto pbegin = p.begin();
            ++pbegin;
            if (std::equal(begin, tup.end(), pbegin,
                           (p.has_values() ? full_eq
                                           : types_only_eq)))
            {
                return {Result::offset_subtuple(tup.vals(), offset)};
            }
        }
        return {};
    }
    template<class Tuple>
    static option<Result> _(Tuple const& tup)
    {
        if (tup.size() >= (sizeof...(T) - 1))
        {
            size_t offset = tup.size() - (sizeof...(T) - 1);
            auto& tarr = static_types_array<T...>::arr;
            for (size_t i = offset, j = 1; i < tup.size(); ++i, ++j)
            {
                if (tarr[j] != tup.type_at(i)) return { };
            }
            return {Result::offset_subtuple(tup.vals(), offset)};
        }
        return {};
    }
};

template<class Result, typename... T>
struct tuple_cast_impl<tuple_cast_impl_id::wildcard_in_between, Result, T...>
{
    static_assert(sizeof...(T) > 1, "tuple_cast<anything> (empty result)");

    typedef util::type_list<T...> types;
    typedef typename util::tl_filter_not<types, util::tbind<std::is_same, anything>::type>::type
            filtered_types;

    // mapping vector type
    static constexpr size_t filtered_size = filtered_types::size;
    typedef util::fixed_vector<size_t, filtered_types::size> mapping_vector;

    // iterator types
    typedef type_value_pair_const_iterator pattern_iterator;
    typedef abstract_tuple::const_iterator const_iterator;

    inline static bool matches(mapping_vector& mv,
                               const_iterator tbegin,
                               const_iterator tend,
                               pattern_iterator pbegin,
                               pattern_iterator pend)
    {
        while (!(pbegin == pend && tbegin == tend))
        {
            if (pbegin == pend)
            {
                // reached end of pattern while some values remain unmatched
                return false;
            }
            else if (pbegin.type() == nullptr) // nullptr == wildcard (anything)
            {
                // perform submatching
                ++pbegin;
                if (pbegin == pend)
                {
                    // always true at the end of the pattern
                    return true;
                }
                // safe current mapping as fallback
                auto s = mv.size();
                // iterate over tuple values until we found a match
                for (; tbegin != tend; ++tbegin)
                {
                    if (matches(mv, tbegin, tend, pbegin, pend)) return true;
                    // restore mapping to fallback (delete invalid mappings)
                    mv.resize(s);
                }
                return false; // no submatch found
            }
            else if (full_eq(tbegin, *pbegin))
            {
                mv.push_back(tbegin.position());
            }
            else
            {
                return false; // no match
            }
            // next iteration
            ++tbegin;
            ++pbegin;
        }
        return true; // pbegin == pend && tbegin == tend
    }

    template<class Tuple>
    inline static option<Result> _(Tuple const& tup, pattern<T...> const& p)
    {
        util::fixed_vector<size_t, Result::num_elements> mv;
        if (matches(mv, tup.begin(), tup.end(), p.begin(), p.end()))
        {
            return {Result::from(tup.vals(), mv)};
        }
        return {};
    }

    template<class Tuple>
    static option<Result> _(Tuple const& tup)
    {
        util::fixed_vector<size_t, Result::num_elements> mv;
        auto& tarr = static_types_array<T...>::arr;
        if (matches(mv, tup.begin(), tup.end(), tarr.begin(), tarr.end()))
        {
            return {Result::from(tup.vals(), mv)};
        }
        return {};
    }
};

template<typename... T>
struct select_tuple_cast_impl
{
    typedef util::type_list<T...> types;
    typedef util::tl_find<types, anything> result1;
    typedef typename result1::rest_list rest_list;

    static constexpr int wc1 = result1::value;

    static constexpr int wc2 =
            (wc1 < 0)
            ? -1
            : (util::tl_find<rest_list, anything>::value + wc1 + 1);

    static constexpr tuple_cast_impl_id value =
            (wc1 == -1)
            ? tuple_cast_impl_id::no_wildcard
            : ((wc1 == 0 && wc2 == -1)
               ? tuple_cast_impl_id::leading_wildcard
               : ((wc1 == (sizeof...(T) - 1) && wc2 == -1)
                  ? tuple_cast_impl_id::trailing_wildcard
                  : tuple_cast_impl_id::wildcard_in_between));
};

} }

#endif // TUPLE_CAST_IMPL_HPP
