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

#include "cppa/pattern.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/detail/types_array.hpp"

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

    Iterator iter;
    MappingVector* mapping;

 public:

    typedef MappingVector mapping_vector;

    push_mapping_decorator(Iterator const& i, MappingVector* mv)
        : iter(i), mapping(mv)
    {
    }

    push_mapping_decorator(push_mapping_decorator const& other, MappingVector* mv)
        : iter(other.iter), mapping(mv)
    {
    }

    push_mapping_decorator(push_mapping_decorator&& other)
        : iter(std::move(other.iter)), mapping(other.mapping)
    {
    }

    inline void next() { iter.next(); }
    inline bool at_end() const { return iter.at_end(); }
    inline void const* value() const { return iter.value(); }
    inline decltype(iter.type()) type() const { return iter.type(); }
    inline bool has_mapping() const { return mapping != nullptr; }
    inline void push_mapping()
    {
        if (mapping) mapping->push_back(iter.position());
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
//int matches(TupleIterator targ, typename pattern<P...>::const_iterator iter)
auto matches(TupleIterator targ, PatternIterator iter)
    -> typename util::enable_if_c<is_pm_decorated<TupleIterator>::value,bool>::type
{
    for ( ; !(iter.at_end() && targ.at_end()); iter.next(), targ.next())
    {
        if (iter.at_end())
        {
            return false;
        }
        else if (iter.type() == nullptr) // nullptr == wildcard (anything)
        {
            // perform submatching
            iter.next();
            if (iter.at_end())
            {
                // always true at the end of the pattern
                return true;
            }
            typename TupleIterator::mapping_vector mv;
            auto mv_ptr = (targ.has_mapping()) ? &mv : nullptr;
            // iterate over tu_args until we found a match
            for ( ; targ.at_end() == false; mv.clear(), targ.next())
            {
                if (matches(TupleIterator(targ, mv_ptr), iter))
                {
                    targ.push_mapping(mv);
                    return true;
                }
            }
            return false; // no submatch found
        }
        // compare types
        else if (targ.at_end() == false && iter.type() == targ.type())
        {
            // compare values if needed
            if (   iter.has_value() == false
                || iter.type()->equals(iter.value(), targ.value()))
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
    return true; // iter.at_end() && targ.at_end()
}

template<class TupleIterator, class PatternIterator>
auto matches(TupleIterator targ, PatternIterator iter)
    -> typename util::enable_if_c<!is_pm_decorated<TupleIterator>::value,bool>::type
{
    for ( ; !(iter.at_end() && targ.at_end()); iter.next(), targ.next())
    {
        if (iter.at_end())
        {
            return false;
        }
        else if (iter.type() == nullptr) // nullptr == wildcard (anything)
        {
            // perform submatching
            iter.next();
            if (iter.at_end())
            {
                // always true at the end of the pattern
                return true;
            }
            // iterate over tu_args until we found a match
            for ( ; targ.at_end() == false; targ.next())
            {
                if (matches(targ, iter))
                {
                    return true;
                }
            }
            return false; // no submatch found
        }
        // compare types
        else if (targ.at_end() == false && iter.type() == targ.type())
        {
            // compare values if needed
            if (   iter.has_value() == false
                || iter.type()->equals(iter.value(), targ.value()))
            {
                // ok, match next
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
    return true; // iter.at_end() && targ.at_end()
}

} } // namespace cppa::detail

#endif // DO_MATCH_HPP
