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


#ifndef DO_MATCH_HPP
#define DO_MATCH_HPP

#include <algorithm>
#include <type_traits>

#include "cppa/pattern.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/decorated_tuple.hpp"

namespace cppa { namespace detail {

template<class MappingVector, typename Iterator>
class push_mapping_decorator
{

    Iterator iter;
    MappingVector& mapping;
    size_t commited_size;

 public:

    typedef MappingVector mapping_vector;

    push_mapping_decorator(Iterator const& i, MappingVector& mv)
        : iter(i), mapping(mv), commited_size(0)
    {
    }

    push_mapping_decorator(push_mapping_decorator const&) = default;

    inline push_mapping_decorator& operator++()
    {
        ++iter;
        return *this;
    }

    inline push_mapping_decorator& operator--()
    {
        --iter;
        return *this;
    }

    inline bool operator==(Iterator const& i) { return iter == i; }

    inline bool operator!=(Iterator const& i) { return iter != i; }

    inline decltype(iter.value()) value() const { return iter.value(); }

    inline decltype(iter.type()) type() const { return iter.type(); }

    inline size_t position() const { return iter.position(); }

    inline void push_mapping()
    {
        mapping.push_back(position());
    }

    inline void commit_mapping()
    {
        commited_size = mapping.size();
    }

    inline void rollback_mapping()
    {
        mapping.resize(commited_size);
    }

};

template<typename Iterator, class MappingVector>
push_mapping_decorator<MappingVector, Iterator> pm_decorated(Iterator i, MappingVector& mv)
{
    return {i, mv};
}

template<typename T>
class is_pm_decorated
{

    template<class C> static bool sfinae_fun(C* cptr,
                                             decltype(cptr->has_mapping())* = 0)
    {
        return true;
    }

    static void sfinae_fun(void*) { }

    typedef decltype(sfinae_fun(static_cast<T*>(nullptr))) result_type;

 public:

    static constexpr bool value = std::is_same<bool, result_type>::value;

};

template<typename T>
inline typename util::disable_if<is_pm_decorated<T>, void>::type
push_mapping(T&) { }

template<typename T>
inline typename util::enable_if<is_pm_decorated<T>, void>::type
push_mapping(T& iter)
{
    iter.push_mapping();
}

template<typename T>
inline typename util::disable_if<is_pm_decorated<T>, void>::type
commit_mapping(T&) { }

template<typename T>
inline typename util::enable_if<is_pm_decorated<T>, void>::type
commit_mapping(T& iter)
{
    iter.commit_mapping();
}

template<typename T>
inline typename util::disable_if<is_pm_decorated<T>, void>::type
rollback_mapping(T&) { }

template<typename T>
inline typename util::enable_if<is_pm_decorated<T>, void>::type
rollback_mapping(T& iter)
{
    iter.rollback_mapping();
}

template<class TupleBeginIterator, class TupleEndIterator, class PatternIterator>
bool matches(TupleBeginIterator tbegin, TupleEndIterator tend,
             PatternIterator pbegin, PatternIterator pend)
{
    while (!(pbegin == pend && tbegin == tend))
    {
        if (pbegin == pend)
        {
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
            commit_mapping(tbegin);
            // iterate over tu_args until we found a match
            for (; tbegin != tend; ++tbegin)
            {
                if (matches(tbegin, tend, pbegin, pend)) return true;
                // restore mapping to fallback (delete invalid mappings)
                rollback_mapping(tbegin);
            }
            return false; // no submatch found
        }
        // compare types
        else if (tbegin != tend && pbegin.type() == tbegin.type())
        {
            // compare values if needed
            if (   pbegin.value() == nullptr
                || pbegin.type()->equals(pbegin.value(), tbegin.value()))
            {
                push_mapping(tbegin);
            }
            else
            {
                return false; // values didn't match
            }
        }
        else
        {
            return false; // no match
        }
    }
    return true; // pbegin == pend && targ.at_end()
}

// pattern does not contain "anything"
template<class Tuple, class Pattern>
bool matches(std::integral_constant<int, 0>,
             Tuple const& tpl, Pattern const& pttrn,
             util::fixed_vector<size_t, Pattern::filtered_size>* mv)
{
    // assertion "tpl.size() == pttrn.size()" is guaranteed from caller
    typedef typename Pattern::types ptypes;
    typedef typename decorated_tuple_from_type_list<ptypes>::type dec_t;
    typedef typename tuple_vals_from_type_list<ptypes>::type tv_t;
    std::type_info const& tinfo = tpl.impl_type();
    if (tinfo == typeid(dec_t) || tinfo == typeid(tv_t))
    {
        if (pttrn.has_values())
        {
            // compare values only (types are guaranteed to be equal)
            auto eq = [](type_value_pair const& lhs, type_value_pair const& rhs)
            {
                // pattern (rhs) does not have to have a value
                return    rhs.second == nullptr
                       || lhs.first->equals(lhs.second, rhs.second);
            };
            if (std::equal(tpl.begin(), tpl.end(), pttrn.begin(), eq) == false)
            {
                // values differ
                return false;
            }
        }
    }
    else if (tinfo == typeid(object_array))
    {
        if (pttrn.has_values())
        {
            // compares type and value
            auto eq = [](type_value_pair const& lhs, type_value_pair const& rhs)
            {
                // pattern (rhs) does not have to have a value
                return    lhs.first == rhs.first
                       && (   rhs.second == nullptr
                           || lhs.first->equals(lhs.second, rhs.second));
            };
            if (std::equal(tpl.begin(), tpl.end(), pttrn.begin(), eq) == false)
            {
                // types or values differ
                return false;
            }
        }
        else
        {
            // compares the types only
            auto eq = [](type_value_pair const& lhs, type_value_pair const& rhs)
            {
                return lhs.first == rhs.first;
            };
            if (std::equal(tpl.begin(), tpl.end(), pttrn.begin(), eq) == false)
            {
                // types differ
                return false;
            }
        }
    }
    else
    {
        return false;
    }
    // ok => match
    if (mv) for (size_t i = 0; i < Pattern::size; ++i) mv->push_back(i);
    return true;
}

// "anything" at end of pattern
template<class Tuple, class Pattern>
bool matches(std::integral_constant<int, 1>,
             Tuple const& tpl, Pattern const& pttrn,
             util::fixed_vector<size_t, Pattern::filtered_size>* mv)
{
    static_assert(Pattern::size > 0, "empty pattern");
    auto end = pttrn.end();
    --end; // iterate until we reach "anything"
    size_t i = 0;
    if (pttrn.has_values())
    {
        for (auto iter = pttrn.begin(); iter != end; ++iter, ++i)
        {
            if (iter->first != tpl.type_at(i)) return false;
            if (iter->second)
            {
                if (iter->first->equals(iter->second, tpl.at(i)) == false)
                    return false;
            }
        }
    }
    else
    {
        for (auto iter = pttrn.begin(); iter != end; ++iter, ++i)
        {
            if (iter->first != tpl.type_at(i)) return false;
        }
    }
    // ok => match, fill vector if needed and pattern is != <anything>
    if (mv)
    {
        // fill vector up to the position of 'anything'
        size_t end = Pattern::size - 1;
        for (size_t i = 0; i < end; ++i) mv->push_back(i);
    }
    return true;
}

// "anything" somewhere in between (ugh!)
template<class Tuple, class Pattern>
bool matches(std::integral_constant<int, 2>,
             Tuple const& tpl, Pattern const& ptrn,
             util::fixed_vector<size_t, Pattern::filtered_size>* mv = 0)
{
    if (mv)
        return matches(pm_decorated(tpl.begin(), *mv), tpl.end(), ptrn.begin(), ptrn.end());
    return matches(tpl.begin(), tpl.end(), ptrn.begin(), ptrn.end());
}

template<class Tuple, class Pattern>
inline bool matches(Tuple const& tpl, Pattern const& pttrn,
                    util::fixed_vector<size_t, Pattern::filtered_size>* mv = 0)
{
    typedef typename Pattern::types ptypes;
    static constexpr int first_anything = util::tl_find<ptypes, anything>::value;
    // impl0 = no anything
    // impl1 = anything at end
    // impl2 = anything somewhere in between
    static constexpr int impl = (first_anything == -1)
            ? 0
            : ((first_anything == ((int) Pattern::size) - 1) ? 1 : 2);
    std::integral_constant<int, impl> token;
    return matches(token, tpl, pttrn, mv);
}

} } // namespace cppa::detail

#endif // DO_MATCH_HPP
