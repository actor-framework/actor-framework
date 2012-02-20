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


#ifndef MATCHES_HPP
#define MATCHES_HPP

#include <type_traits>

#include "cppa/pattern.hpp"
#include "cppa/anything.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/util/tbind.hpp"
#include "cppa/util/type_list.hpp"

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/decorated_tuple.hpp"

namespace cppa {

enum class pattern_characteristic
{
    no_wildcard,
    trailing_wildcard,
    leading_wildcard,
    wildcard_in_between,
    multiple_wildcards
};

template<typename Types>
constexpr pattern_characteristic get_pattern_characteristic()
{
    return util::tl_exists<Types, util::tbind<std::is_same, anything>::type>::value
           ? ((util::tl_count<Types, util::tbind<std::is_same, anything>::type>::value == 1)
              ? (std::is_same<typename Types::head, anything>::value
                 ? pattern_characteristic::leading_wildcard
                 : (std::is_same<typename Types::back, anything>::value
                    ? pattern_characteristic::trailing_wildcard
                    : pattern_characteristic::wildcard_in_between))
              : pattern_characteristic::multiple_wildcards)
           : pattern_characteristic::no_wildcard;
}

namespace detail {
template<pattern_characteristic, typename...> struct matcher;
} // namespace cppa

template<pattern_characteristic PC, typename... Ts>
struct match_impl
{
    static inline bool _(any_tuple const& tup)
    {
        return detail::matcher<PC, Ts...>::tmatch(tup);
    }
    template<size_t Size>
    static inline bool _(any_tuple const& tup,
                         util::fixed_vector<size_t, Size>& mv)
    {
        return detail::matcher<PC, Ts...>::tmatch(tup, mv);
    }
    static inline bool _(any_tuple const& tup,
                         pattern<Ts...> const& p)
    {
        return    detail::matcher<PC, Ts...>::tmatch(tup)
               && (   p.has_values() == false
                   || detail::matcher<PC, Ts...>::vmatch(tup, p));
    }
    static inline bool _(any_tuple const& tup,
                         pattern<Ts...> const& p,
                         typename pattern<Ts...>::mapping_vector& mv)
    {
        return    detail::matcher<PC, Ts...>::tmatch(tup, mv)
               && (   p.has_values() == false
                   || detail::matcher<PC, Ts...>::vmatch(tup, p));
    }
};

template<typename... Ts>
struct match_impl<pattern_characteristic::multiple_wildcards, Ts...>
{
    static constexpr auto PC = pattern_characteristic::multiple_wildcards;
    static inline bool _(any_tuple const& tup)
    {
        return detail::matcher<PC, Ts...>::tmatch(tup);
    }
    template<size_t Size>
    static inline bool _(any_tuple const& tup,
                         util::fixed_vector<size_t, Size>& mv)
    {
        return detail::matcher<PC, Ts...>::tmatch(tup, mv);
    }
    static inline bool _(any_tuple const& tup, pattern<Ts...> const& p)
    {
        if (p.has_values())
        {
            typename pattern<Ts...>::mapping_vector mv;
            return    detail::matcher<PC, Ts...>::tmatch(tup, mv)
                   && detail::matcher<PC, Ts...>::vmatch(tup, p, mv);
        }
        return detail::matcher<PC, Ts...>::tmatch(tup);
    }
    static inline bool _(any_tuple const& tup, pattern<Ts...> const& p,
                         typename pattern<Ts...>::mapping_vector& mv)
    {
        if (p.has_values())
        {
            typename pattern<Ts...>::mapping_vector mv;
            return    detail::matcher<PC, Ts...>::tmatch(tup, mv)
                   && detail::matcher<PC, Ts...>::vmatch(tup, p, mv);
        }
        return detail::matcher<PC, Ts...>::tmatch(tup, mv);
    }
};

template<typename... Ts>
inline bool match(any_tuple const& tup)
{
    typedef util::type_list<Ts...> tl;
    return match_impl<get_pattern_characteristic<tl>(), Ts...>::_(tup);
}

template<typename... Ts>
inline bool match(any_tuple const& tup,
                  util::fixed_vector<
                    size_t,
                    util::tl_count_not<
                        util::type_list<Ts...>,
                        util::tbind<std::is_same, anything>::type>::value>& mv)
{
    typedef util::type_list<Ts...> tl;
    return match_impl<get_pattern_characteristic<tl>(), Ts...>::_(tup, mv);
}

template<typename... Ts>
inline bool match(any_tuple const& tup, pattern<Ts...> const& ptrn)
{
    typedef util::type_list<Ts...> tl;
    return match_impl<get_pattern_characteristic<tl>(), Ts...>::_(tup, ptrn);
}

template<typename... Ts>
inline bool match(any_tuple const& tup,
                  pattern<Ts...> const& ptrn,
                  typename pattern<Ts...>::mapping_vector& mv)
{
    typedef util::type_list<Ts...> tl;
    return match_impl<get_pattern_characteristic<tl>(), Ts...>::_(tup,
                                                                  ptrn,
                                                                  mv);
}

/******************************************************************************\
**                           implementation details                           **
\******************************************************************************/


namespace detail {

template<typename... T>
struct matcher<pattern_characteristic::no_wildcard, T...>
{
    static inline bool tmatch(any_tuple const& tup)
    {
        auto& impl_typeid = tup.impl_type();
        if (   impl_typeid == typeid(tuple_vals<T...>)
            || impl_typeid == typeid(decorated_tuple<T...>))
        {
            return true;
        }
        else if (   impl_typeid == typeid(detail::object_array)
                 && tup.size() == sizeof...(T))
        {
            auto& tarr = detail::static_types_array<T...>::arr;
            return std::equal(tup.begin(), tup.end(), tarr.begin(),
                              detail::types_only_eq);
        }
        return false;
    }
    static inline bool tmatch(any_tuple const& tup,
                              util::fixed_vector<size_t, sizeof...(T)>& mv)
    {
        if (tmatch(tup))
        {
            size_t i = 0;
            mv.resize(sizeof...(T));
            std::generate(mv.begin(), mv.end(), [&]() { return i++; });
            return true;
        }
        return false;
    }
    static inline bool vmatch(any_tuple const& tup, pattern<T...> const& ptrn)
    {
        return std::equal(tup.begin(), tup.end(), ptrn.begin(),
                          detail::values_only_eq);
    }
};

template<typename... T>
struct matcher<pattern_characteristic::trailing_wildcard, T...>
{
    static constexpr size_t size = sizeof...(T) - 1;
    static inline bool tmatch(any_tuple const& tup)
    {
        if (tup.size() >= size)
        {
            auto& tarr = static_types_array<T...>::arr;
            auto begin = tup.begin();
            return std::equal(begin, begin + size, tarr.begin(),
                              detail::types_only_eq);
        }
        return false;
    }
    static inline bool tmatch(any_tuple const& tup,
                             util::fixed_vector<size_t, size>& mv)
    {
        if (tmatch(tup))
        {
            size_t i = 0;
            mv.resize(size);
            std::generate(mv.begin(), mv.end(), [&]() { return i++; });
            return true;
        }
        return false;
    }
    static inline bool vmatch(any_tuple const& tup, pattern<T...> const& ptrn)
    {
        auto begin = tup.begin();
        return std::equal(begin, begin + size, ptrn.begin(),
                          detail::values_only_eq);
    }
};

template<>
struct matcher<pattern_characteristic::leading_wildcard, anything>
{
    static inline bool tmatch(any_tuple const&)
    {
        return true;
    }
    static inline bool tmatch(any_tuple const&, util::fixed_vector<size_t, 0>&)
    {
        return true;
    }
    static inline bool vmatch(any_tuple const&, pattern<anything> const&)
    {
        return true;
    }
};

template<typename... T>
struct matcher<pattern_characteristic::leading_wildcard, T...>
{
    static constexpr size_t size = sizeof...(T) - 1;
    static inline bool tmatch(any_tuple const& tup)
    {
        auto tup_size = tup.size();
        if (tup_size >= size)
        {
            auto& tarr = static_types_array<T...>::arr;
            auto begin = tup.begin();
            begin += (tup_size - size);
            return std::equal(begin, tup.end(), tarr.begin(),
                              detail::types_only_eq);
        }
        return false;
    }
    static inline bool tmatch(any_tuple const& tup,
                             util::fixed_vector<size_t, size>& mv)
    {
        if (tmatch(tup))
        {
            size_t i = tup.size() - size;
            mv.resize(size);
            std::generate(mv.begin(), mv.end(), [&]() { return i++; });
            return true;
        }
        return false;
    }
    static inline bool vmatch(any_tuple const& tup, pattern<T...> const& ptrn)
    {
        auto begin = tup.begin();
        begin += (tup.size() - size);
        return std::equal(begin, tup.end(), ptrn.begin() + 1,
                          detail::values_only_eq);
    }
};

template<typename... T>
struct matcher<pattern_characteristic::wildcard_in_between, T...>
{
    static constexpr int signed_wc_pos =
            util::tl_find<util::type_list<T...>, anything>::value;
    static_assert(   signed_wc_pos != -1
                  && signed_wc_pos != 0
                  && signed_wc_pos != (sizeof...(T) - 1),
                  "illegal wildcard position");
    static constexpr size_t wc_pos = static_cast<size_t>(signed_wc_pos);
    static constexpr size_t size = sizeof...(T) - 1;
    static inline bool tmatch(any_tuple const& tup)
    {
        auto tup_size = tup.size();
        if (tup_size >= size)
        {
            auto& tarr = static_types_array<T...>::arr;
            // first range [0, X1)
            auto begin = tup.begin();
            auto end = begin + wc_pos;
            if (std::equal(begin, end, tarr.begin(), detail::types_only_eq))
            {
                // second range [X2, N)
                begin = end = tup.end();
                begin -= (size - (wc_pos + 1));
                auto arr_begin = tarr.begin() + wc_pos + 1;
                return std::equal(begin, end, arr_begin, detail::types_only_eq);
            }
        }
        return false;
    }
    static inline bool tmatch(any_tuple const& tup,
                             util::fixed_vector<size_t, size>& mv)
    {
        if (tmatch(tup))
        {
            // first range
            size_t i = 0;
            mv.resize(size);
            auto begin = mv.begin();
            std::generate(begin, begin + wc_pos, [&]() { return i++; });
            // second range
            i = tup.size() - (size - (wc_pos + 1));
            begin = mv.begin() + wc_pos;
            std::generate(begin, mv.end(), [&]() { return i++; });
            return true;
        }
        return false;
    }
    static inline bool vmatch(any_tuple const& tup, pattern<T...> const& ptrn)
    {
        // first range
        auto tbegin = tup.begin();
        auto tend = tbegin + wc_pos;
        if (std::equal(tbegin, tend, ptrn.begin(), detail::values_only_eq))
        {
            // second range
            tbegin = tend = tup.end();
            tbegin -= (size - (wc_pos + 1));
            auto pbegin = ptrn.begin();
            pbegin += (wc_pos + 1);
            return std::equal(tbegin, tend, pbegin, detail::values_only_eq);
        }
        return false;
    }
};

template<typename... T>
struct matcher<pattern_characteristic::multiple_wildcards, T...>
{
    static constexpr size_t wc_count =
            util::tl_count<util::type_list<T...>,
                           util::tbind<std::is_same, anything>::type>::value;
    static_assert(sizeof...(T) > wc_count, "only wildcards given");

    template<class TupleIter, class PatternIter,
             class Push, class Commit, class Rollback>
    static bool match(TupleIter tbegin, TupleIter tend,
                      PatternIter pbegin, PatternIter pend,
                      Push&& push, Commit&& commit, Rollback&& rollback)
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
                // always true at the end of the pattern
                if (pbegin == pend) return true;
                // safe current mapping as fallback
                commit();
                // iterate over tuple values until we found a match
                for (; tbegin != tend; ++tbegin)
                {
                    if (match(tbegin, tend, pbegin, pend,
                              push, commit, rollback))
                    {
                        return true;
                    }
                    // restore mapping to fallback (delete invalid mappings)
                    rollback();
                }
                return false; // no submatch found
            }
            // compare types
            else if (tbegin.type() == pbegin.type()) push(tbegin);
            // no match
            else return false;
            // next iteration
            ++tbegin;
            ++pbegin;
        }
        return true; // pbegin == pend && tbegin == tend
    }

    static inline bool tmatch(any_tuple const& tup)
    {
        auto& tarr = static_types_array<T...>::arr;
        if (tup.size() >= (sizeof...(T) - wc_count))
        {
            return match(tup.begin(), tup.end(), tarr.begin(), tarr.end(),
                         [](any_tuple::const_iterator const&) { },
                         []() { },
                         []() { });
        }
        return false;
    }

    template<class MappingVector>
    static inline bool tmatch(any_tuple const& tup, MappingVector& mv)
    {
        auto& tarr = static_types_array<T...>::arr;
        if (tup.size() >= (sizeof...(T) - wc_count))
        {
            size_t commited_size = 0;
            return match(tup.begin(), tup.end(), tarr.begin(), tarr.end(),
                         [&](any_tuple::const_iterator const& iter)
                         {
                             mv.push_back(iter.position());
                         },
                         [&]() { commited_size = mv.size(); },
                         [&]() { mv.resize(commited_size); });
        }
        return false;
    }
    static inline bool vmatch(any_tuple const& tup,
                              pattern<T...> const& ptrn,
                              typename pattern<T...>::mapping_vector const& mv)
    {
        auto i = mv.begin();
        for (auto j = ptrn.begin(); j != ptrn.end(); ++j)
        {
            if (j.type() != nullptr)
            {
                if (   j.value() != nullptr
                    && j.type()->equals(tup.at(*i), j.value()) == false)
                {
                    return false;
                }
                ++i;
            }
        }
    }
};

} // namespace detail

} // namespace cppa

#endif // MATCHES_HPP
