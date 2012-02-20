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


#ifndef DECORATED_TUPLE_HPP
#define DECORATED_TUPLE_HPP

#include <vector>
#include <algorithm>

#include "cppa/config.hpp"
#include "cppa/cow_ptr.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/type_list.hpp"
#include "cppa/util/apply_tuple.hpp"
#include "cppa/util/fixed_vector.hpp"

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/serialize_tuple.hpp"

namespace cppa { namespace detail {

template<typename... ElementTypes>
class decorated_tuple : public abstract_tuple
{

    static_assert(sizeof...(ElementTypes) > 0,
                  "decorated_tuple is not allowed to be empty");

 public:

    typedef util::fixed_vector<size_t, sizeof...(ElementTypes)> vector_type;

    typedef cow_ptr<abstract_tuple> cow_pointer_type;

    static cow_pointer_type create(cow_pointer_type d, vector_type const& v)
    {
        return {(new decorated_tuple(std::move(d)))->init(v)};
    }

    // creates a subtuple form @p d with sizeof...(ElementTypes) elements
    static cow_pointer_type create(cow_pointer_type d)
    {
        return {(new decorated_tuple(std::move(d)))->init()};
    }

    // creates a subtuple form @p d with an offset
    static cow_pointer_type create(cow_pointer_type d, size_t offset)
    {
        return {(new decorated_tuple(std::move(d)))->init(offset)};
    }

    virtual void* mutable_at(size_t pos)
    {
        CPPA_REQUIRE(pos < size());
        CPPA_REQUIRE(m_mapping[pos] < m_decorated->size());
        return m_decorated->mutable_at(m_mapping[pos]);
    }

    virtual size_t size() const
    {
        return sizeof...(ElementTypes);
    }

    virtual decorated_tuple* copy() const
    {
        return new decorated_tuple(*this);
    }

    virtual void const* at(size_t pos) const
    {
        CPPA_REQUIRE(pos < size());
        CPPA_REQUIRE(m_mapping[pos] < m_decorated->size());
        return m_decorated->at(m_mapping[pos]);
    }

    virtual uniform_type_info const* type_at(size_t pos) const
    {
        CPPA_REQUIRE(pos < size());
        CPPA_REQUIRE(m_mapping[pos] < m_decorated->size());
        return m_decorated->type_at(m_mapping[pos]);
    }

    virtual std::type_info const& impl_type() const
    {
        return typeid(decorated_tuple);
    }

    cow_pointer_type& decorated()
    {
        return m_decorated;
    }

    cow_pointer_type const& decorated() const
    {
        return m_decorated;
    }

 private:

    cow_pointer_type m_decorated;
    vector_type m_mapping;

    decorated_tuple(cow_pointer_type&& d) : m_decorated(std::move(d)) { }

    decorated_tuple(decorated_tuple const&) = default;

    decorated_tuple* init(vector_type const& v)
    {
        CPPA_REQUIRE(m_decorated->size() >= sizeof...(ElementTypes));
        CPPA_REQUIRE(v.size() == sizeof...(ElementTypes));
        CPPA_REQUIRE(*(std::max_element(v.begin(), v.end())) < m_decorated->size());
        m_mapping.resize(v.size());
        std::copy(v.begin(), v.end(), m_mapping.begin());
        return this;
    }

    decorated_tuple* init()
    {
        CPPA_REQUIRE(m_decorated->size() >= sizeof...(ElementTypes));
        size_t i = 0;
        m_mapping.resize(sizeof...(ElementTypes));
        std::generate(m_mapping.begin(), m_mapping.end(), [&]() {return i++;});
        return this;
    }

    decorated_tuple* init(size_t offset)
    {
        CPPA_REQUIRE((m_decorated->size() - offset) == sizeof...(ElementTypes));
        size_t i = offset;
        m_mapping.resize(sizeof...(ElementTypes));
        std::generate(m_mapping.begin(), m_mapping.end(), [&]() {return i++;});
        return this;
    }

    decorated_tuple& operator=(decorated_tuple const&) = delete;

};

template<typename TypeList>
struct decorated_tuple_from_type_list;

template<typename... Types>
struct decorated_tuple_from_type_list< util::type_list<Types...> >
{
    typedef decorated_tuple<Types...> type;
};

} } // namespace cppa::detail

#endif // DECORATED_TUPLE_HPP
