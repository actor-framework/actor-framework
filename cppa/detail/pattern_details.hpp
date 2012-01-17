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


#ifndef PATTER_DETAILS_HPP
#define PATTER_DETAILS_HPP

#include <type_traits>

#include "cppa/anything.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/any_tuple_iterator.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/unboxed.hpp"

namespace cppa { namespace detail {

template<class TypesArray>
class pattern_iterator
{

    size_t m_pos;
    size_t m_size;
    void const* const* m_data;
    TypesArray const& m_types;

 public:

    inline pattern_iterator(size_t msize,
                            void const* const* mdata,
                            TypesArray const& mtypes)
        : m_pos(0)
        , m_size(msize)
        , m_data(mdata)
        , m_types(mtypes)
    {
    }

    pattern_iterator(pattern_iterator const&) = default;

    pattern_iterator& operator=(pattern_iterator const&) = default;

    inline bool at_end() const
    {
        return m_pos == m_size;
    }

    inline void next()
    {
        ++m_pos;
    }

    inline uniform_type_info const* type() const
    {
        return m_types[m_pos];
    }

    inline void const* value() const
    {
        return m_data[m_pos];
    }

    inline bool has_value() const
    {
        return value() != nullptr;
    }

};

template<class T, class VectorType>
class tuple_iterator_arg
{

    size_t pos;
    std::type_info const* element_type;
    typename T::const_iterator i;
    typename T::const_iterator end;
    VectorType* mapping;

 public:

    inline tuple_iterator_arg(T const& iterable, VectorType* mv = nullptr)
        : pos(0), element_type(&typeid(typename T::value_type)), i(iterable.begin()), end(iterable.end()), mapping(mv)
    {
    }

    inline tuple_iterator_arg(tuple_iterator_arg const& other, VectorType* mv)
        : pos(other.pos), element_type(other.element_type), i(other.i), end(other.end), mapping(mv)
    {
    }

    inline bool at_end() const
    {
        return i == end;
    }

    inline void next()
    {
        ++pos;
        ++i;
    }

    inline bool has_mapping() const
    {
        return mapping != nullptr;
    }

    inline void push_mapping()
    {
        if (mapping) mapping->push_back(pos);
    }

    inline void push_mapping(VectorType const& what)
    {
        if (mapping)
        {
            mapping->insert(mapping->end(), what.begin(), what.end());
        }
    }

    inline std::type_info const& type() const
    {
        return *element_type;
    }

    inline void const* value() const
    {
        return &(*i);
    }

};

template<class VectorType>
class tuple_iterator_arg<any_tuple, VectorType>
{

    util::any_tuple_iterator iter;
    VectorType* mapping;

 public:

    inline tuple_iterator_arg(any_tuple const& tup, VectorType* mv = nullptr)
        : iter(tup), mapping(mv)
    {
    }

    inline tuple_iterator_arg(tuple_iterator_arg const& other, VectorType* mv)
        : iter(other.iter), mapping(mv)
    {
    }

    inline bool at_end() const
    {
        return iter.at_end();
    }

    inline void next()
    {
        iter.next();
    }

    inline bool has_mapping() const
    {
        return mapping != nullptr;
    }

    inline void push_mapping()
    {
        if (mapping) mapping->push_back(iter.position());
    }

    inline void push_mapping(VectorType const& what)
    {
        if (mapping)
        {
            mapping->insert(mapping->end(), what.begin(), what.end());
        }
    }

    inline auto type() const -> decltype(iter.type())
    {
        return iter.type();
    }

    inline void const* value() const
    {
        return iter.value_ptr();
    }

};

template<class TypesArray, class T, class VectorType>
bool do_match(pattern_iterator<TypesArray>& iter, tuple_iterator_arg<T, VectorType>& targ)
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
            VectorType mv;
            auto mv_ptr = (targ.has_mapping()) ? &mv : nullptr;
            // iterate over tu_args until we found a match
            for ( ; targ.at_end() == false; mv.clear(), targ.next())
            {
                auto iter_cpy = iter;
                tuple_iterator_arg<T,VectorType> targ_cpy(targ, mv_ptr);
                if (do_match(iter_cpy, targ_cpy))
                {
                    targ.push_mapping(mv);
                    return true;
                }
            }
            return false; // no submatch found
        }
        // compare types
        else if (targ.at_end() == false && *(iter.type()) == targ.type())
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

} } // namespace cppa::detail


#endif // PATTER_DETAILS_HPP
