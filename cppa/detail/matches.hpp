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

#include <type_traits>

#include "cppa/pattern.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/decorated_tuple.hpp"

namespace cppa { namespace detail {

template<class T, typename Iterator>
class cppa_iterator_decorator
{

    static types_array<T> tarr;

    Iterator begin;
    Iterator end;
    size_t pos;
    uniform_type_info const* t;

 public:

    cppa_iterator_decorator(Iterator first, Iterator last)
        : begin(first), end(last), pos(0), t(tarr[0])
    {
    }

    inline void next()
    {
        ++begin;
        ++pos;
    }

    inline bool at_end() const { return begin == end; }

    inline uniform_type_info const* type() const { return t; }

    inline void const* value() const { return &(*begin); }

    inline size_t position() const { return pos; }

};

template<class T, typename Iterator>
types_array<T> cppa_iterator_decorator<T, Iterator>::tarr;

template<class MappingVector, typename Iterator>
class push_mapping_decorator
{

    size_t pos;
    Iterator iter;
    MappingVector* mapping;

 public:

    typedef MappingVector mapping_vector;

    push_mapping_decorator(Iterator const& i, MappingVector* mv)
        : pos(0), iter(i), mapping(mv)
    {
    }

    push_mapping_decorator(push_mapping_decorator const& other,
                           MappingVector* mv)
        : pos(other.pos), iter(other.iter), mapping(mv)
    {
    }

    push_mapping_decorator(push_mapping_decorator&& other)
        : pos(other.pos), iter(std::move(other.iter)), mapping(other.mapping)
    {
    }

    inline void next() { ++pos; iter.next(); }
    inline bool at_end() const { return iter.at_end(); }
    inline void const* value() const { return iter.value(); }
    inline decltype(iter.type()) type() const { return iter.type(); }
    inline bool has_mapping() const { return mapping != nullptr; }
    inline void push_mapping()
    {
        if (mapping) mapping->push_back(pos);
    }
    inline void push_mapping(MappingVector const& mv)
    {
        if (mapping) mapping->insert(mapping->end(), mv.begin(), mv.end());
    }

};

template<typename Iterator, class MappingVector>
push_mapping_decorator<MappingVector, Iterator> pm_decorated(Iterator i, MappingVector* mv)
{
    return {i, mv};
}

template<typename Iterator>
auto ci_decorated(Iterator begin, Iterator end)
     -> cppa_iterator_decorator<typename util::rm_ref<decltype(*begin)>::type,
                                Iterator>
{
    return {begin, end};
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

template<class TupleIterator, class PatternIterator>
auto matches(TupleIterator targ, PatternIterator pbegin, PatternIterator pend)
    -> typename util::enable_if_c<is_pm_decorated<TupleIterator>::value,bool>::type
{
    for ( ; !(pbegin == pend && targ.at_end()); ++pbegin, targ.next())
    {
        if (pbegin == pend)
        {
            return false;
        }
        else if (pbegin->first == nullptr) // nullptr == wildcard (anything)
        {
            // perform submatching
            ++pbegin;
            if (pbegin == pend)
            {
                // always true at the end of the pattern
                return true;
            }
            typename TupleIterator::mapping_vector mv;
            auto mv_ptr = (targ.has_mapping()) ? &mv : nullptr;
            // iterate over tu_args until we found a match
            for ( ; targ.at_end() == false; mv.clear(), targ.next())
            {
                if (matches(TupleIterator(targ, mv_ptr), pbegin, pend))
                {
                    targ.push_mapping(mv);
                    return true;
                }
            }
            return false; // no submatch found
        }
        // compare types
        else if (targ.at_end() == false && pbegin->first == targ.type())
        {
            // compare values if needed
            if (   pbegin->second == nullptr
                || pbegin->first->equals(pbegin->second, targ.value()))
            {
                targ.push_mapping();
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

template<class TupleIterator, class PatternIterator>
auto matches(TupleIterator targ, PatternIterator pbegin, PatternIterator pend)
    -> typename util::enable_if_c<!is_pm_decorated<TupleIterator>::value,bool>::type
{
    for ( ; !(pbegin == pend && targ.at_end()); ++pbegin, targ.next())
    {
        if (pbegin == pend)
        {
            return false;
        }
        else if (pbegin->first == nullptr) // nullptr == wildcard (anything)
        {
            // perform submatching
            ++pbegin;
            if (pbegin == pend)
            {
                // always true at the end of the pattern
                return true;
            }
            // iterate over tu_args until we found a match
            for ( ; targ.at_end() == false; targ.next())
            {
                if (matches(targ, pbegin, pend))
                {
                    return true;
                }
            }
            return false; // no submatch found
        }
        // compare types
        else if (targ.at_end() == false && pbegin->first == targ.type())
        {
            // compare values if needed
            if (   pbegin->second != nullptr
                && !pbegin->first->equals(pbegin->second, targ.value()))
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
    typedef typename Pattern::types ptypes;
    typedef typename decorated_tuple_from_type_list<ptypes>::type dec_t;
    typedef typename tuple_vals_from_type_list<ptypes>::type tv_t;
    std::type_info const& tinfo = tpl.impl_type();
    if (tinfo == typeid(dec_t) || tinfo == typeid(tv_t))
    {
        if (pttrn.has_values())
        {
            size_t i = 0;
            auto end = pttrn.end();
            for (auto iter = pttrn.begin(); iter != end; ++iter)
            {
                if (iter->second)
                {
                    if (iter->first->equals(iter->second, tpl.at(i)) == false)
                    {
                        return false;
                    }
                }
                ++i;
            }
        }
    }
    else if (tinfo == typeid(object_array))
    {
        if (pttrn.has_values())
        {
            size_t i = 0;
            auto end = pttrn.end();
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
            size_t i = 0;
            auto end = pttrn.end();
            for (auto iter = pttrn.begin(); iter != end; ++iter, ++i)
            {
                if (iter->first != tpl.type_at(i)) return false;
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
    // ok => match
    if (mv) for (size_t i = 0; i < Pattern::size; ++i) mv->push_back(i);
    return true;
}

// "anything" somewhere in between (ugh!)
template<class Tuple, class Pattern>
bool matches(std::integral_constant<int, 2>,
             Tuple const& tpl, Pattern const& ptrn,
             util::fixed_vector<size_t, Pattern::filtered_size>* mv = 0)
{
    if (mv)
        return matches(pm_decorated(tpl.begin(), mv), ptrn.begin(), ptrn.end());
    return matches(tpl.begin(), ptrn.begin(), ptrn.end());
}

template<class Tuple, class Pattern>
inline bool matches(Tuple const& tpl, Pattern const& pttrn,
                    util::fixed_vector<size_t, Pattern::filtered_size>* mv = 0)
{
    typedef typename Pattern::types ptypes;
    static constexpr int first_anything = util::tl_find<ptypes, anything>::value;
    // impl1 = no anything
    // impl2 = anything at end
    // impl3 = anything somewhere in between
    static constexpr int impl = (first_anything == -1)
            ? 0
            : ((first_anything == ((int) Pattern::size) - 1) ? 1 : 2);
    static constexpr std::integral_constant<int, impl> token;
    return matches(token, tpl, pttrn, mv);
}

} } // namespace cppa::detail

#endif // DO_MATCH_HPP
