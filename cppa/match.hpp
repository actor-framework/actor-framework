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
#include "cppa/util/type_list.hpp"

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/decorated_tuple.hpp"

namespace cppa {

namespace detail {

typedef std::integral_constant<wildcard_position,
                               wildcard_position::nil>
        no_wildcard;

bool tmatch(no_wildcard,
            std::type_info const* tuple_value_type_list,
            any_tuple::const_iterator const& tuple_begin,
            any_tuple::const_iterator const& tuple_end,
            std::type_info const* pattern_type_list,
            type_value_pair_const_iterator pattern_begin,
            type_value_pair_const_iterator pattern_end);

} // namespace detail

namespace detail {
template<wildcard_position, typename...> struct matcher;
} // namespace detail

template<wildcard_position PC, typename... Ts>
struct match_impl;

/**
 *
 */
template<typename... Ts>
inline bool match(any_tuple const& tup)
{
    typedef util::type_list<Ts...> tl;
    return match_impl<get_wildcard_position<tl>(), Ts...>::_(tup);
}

/**
 *
 */
template<typename... Ts>
inline bool match(any_tuple const& tup,
                  util::fixed_vector<
                      size_t,
                      util::tl_count_not<
                          util::type_list<Ts...>,
                          is_anything>::value>& mv)
{
    typedef util::type_list<Ts...> tl;
    return match_impl<get_wildcard_position<tl>(), Ts...>::_(tup, mv);
}

/**
 *
 */
template<typename... Ts>
inline bool match(any_tuple const& tup, pattern<Ts...> const& ptrn)
{
    typedef util::type_list<Ts...> tl;
    return match_impl<get_wildcard_position<tl>(), Ts...>::_(tup, ptrn);
}

/**
 *
 */
template<typename... Ts>
inline bool match(any_tuple const& tup,
                  pattern<Ts...> const& ptrn,
                  typename pattern<Ts...>::mapping_vector& mv)
{
    typedef util::type_list<Ts...> tl;
    return match_impl<get_wildcard_position<tl>(), Ts...>::_(tup,
                                                                  ptrn,
                                                                  mv);
}

// implementation for zero or one wildcards
template<wildcard_position PC, typename... Ts>
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

// implementation for multiple wildcards
template<typename... Ts>
struct match_impl<wildcard_position::multiple, Ts...>
{
    static constexpr auto PC = wildcard_position::multiple;

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

/******************************************************************************\
**                           implementation details                           **
\******************************************************************************/


namespace detail {

template<typename... T>
struct matcher<wildcard_position::nil, T...>
{
    static inline bool tmatch(any_tuple const& tup)
    {
        // match implementation type if possible
        auto vals = tup.values_type_list();
        auto prns = detail::static_type_list<T...>::list;
        if (vals == prns || *vals == *prns)
        {
            return true;
        }
        // always use a full dynamic match for object arrays
        else if (*vals == typeid(detail::object_array)
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
            mv.resize(sizeof...(T));
            std::iota(mv.begin(), mv.end(), 0);
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
struct matcher<wildcard_position::trailing, T...>
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
            mv.resize(size);
            std::iota(mv.begin(), mv.end(), 0);
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
struct matcher<wildcard_position::leading, anything>
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
struct matcher<wildcard_position::leading, T...>
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
            return std::equal(begin, tup.end(),
                              (tarr.begin() + 1), // skip 'anything'
                              detail::types_only_eq);
        }
        return false;
    }

    static inline bool tmatch(any_tuple const& tup,
                              util::fixed_vector<size_t, size>& mv)
    {
        if (tmatch(tup))
        {
            mv.resize(size);
            std::iota(mv.begin(), mv.end(), tup.size() - size);
            return true;
        }
        return false;
    }

    static inline bool vmatch(any_tuple const& tup, pattern<T...> const& ptrn)
    {
        auto begin = tup.begin();
        begin += (tup.size() - size);
        return std::equal(begin, tup.end(),
                          ptrn.begin() + 1,  // skip 'anything'
                          detail::values_only_eq);
    }
};

template<typename... T>
struct matcher<wildcard_position::in_between, T...>
{
    static constexpr int signed_wc_pos =
            util::tl_find<util::type_list<T...>, anything>::value;
    static constexpr size_t size = sizeof...(T);
    static constexpr size_t wc_pos = static_cast<size_t>(signed_wc_pos);

    static_assert(   signed_wc_pos != -1
                  && signed_wc_pos != 0
                  && signed_wc_pos != (sizeof...(T) - 1),
                  "illegal wildcard position");

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
                auto arr_begin = tarr.begin() + (wc_pos + 1);
                return std::equal(begin, end, arr_begin, detail::types_only_eq);
            }
        }
        return false;
    }

    static inline bool tmatch(any_tuple const& tup,
                              util::fixed_vector<size_t, size - 1>& mv)
    {
        if (tmatch(tup))
        {
            // first range
            mv.resize(size - 1);
            auto begin = mv.begin();
            std::iota(begin, begin + wc_pos, 0);
            // second range
            begin = mv.begin() + wc_pos;
            std::iota(begin, mv.end(), tup.size() - (size - (wc_pos + 1)));
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
struct matcher<wildcard_position::multiple, T...>
{
    static constexpr size_t wc_count =
            util::tl_count<util::type_list<T...>, is_anything>::value;

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
