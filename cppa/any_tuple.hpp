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


#ifndef ANY_TUPLE_HPP
#define ANY_TUPLE_HPP

#include "cppa/cow_ptr.hpp"

#include "cppa/tuple.hpp"
#include "cppa/detail/abstract_tuple.hpp"

namespace cppa {

/**
 * @brief Describes a fixed-length tuple with elements of any type.
 */
class any_tuple
{

    cow_ptr<detail::abstract_tuple> m_vals;

    explicit any_tuple(cow_ptr<detail::abstract_tuple> const& vals);

 public:

    any_tuple();

    template<typename... Args>
    any_tuple(tuple<Args...> const& t) : m_vals(t.vals()) { }

    explicit any_tuple(detail::abstract_tuple*);

    any_tuple(any_tuple&&);

    any_tuple(any_tuple const&) = default;

    any_tuple& operator=(any_tuple&&);

    any_tuple& operator=(any_tuple const&) = default;

    size_t size() const;

    void* mutable_at(size_t p);

    void const* at(size_t p) const;

    uniform_type_info const* type_at(size_t p) const;

    cow_ptr<detail::abstract_tuple> const& vals() const;

    bool equals(any_tuple const& other) const;

    any_tuple tail(size_t offset = 1) const;

    inline bool empty() const
    {
        return size() == 0;
    }

    template<typename T>
    inline T const& get_as(size_t p) const;

    template<typename T>
    inline T& get_mutable_as(size_t p);

    class const_iterator
    {

        any_tuple const& ref;
        size_t p;

     public:

        inline const_iterator(any_tuple const& data, size_t pos = 0)
            : ref(data), p(pos)
        {
        }

        inline void next() { ++p; }
        inline bool at_end() const { return p >= ref.size(); }
        inline size_t position() const { return p; }
        inline void const* value() const { return ref.at(p); }
        inline uniform_type_info const* type() const { return ref.type_at(p); }

    };

    inline const_iterator begin() const { return {*this}; }

};

inline bool operator==(any_tuple const& lhs, any_tuple const& rhs)
{
    return lhs.equals(rhs);
}

inline bool operator!=(any_tuple const& lhs, any_tuple const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
inline T const& any_tuple::get_as(size_t p) const
{
    return *reinterpret_cast<T const*>(at(p));
}

template<typename T>
inline T& any_tuple::get_mutable_as(size_t p)
{
    return *reinterpret_cast<T*>(mutable_at(p));
}

} // namespace cppa

#endif // ANY_TUPLE_HPP
