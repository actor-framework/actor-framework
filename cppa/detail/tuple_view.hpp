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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#ifndef CPPA_TUPLE_VIEW_HPP
#define CPPA_TUPLE_VIEW_HPP

#include "cppa/guard_expr.hpp"

#include "cppa/util/rebindable_reference.hpp"

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/abstract_tuple.hpp"

namespace cppa { namespace detail {

struct tuple_view_copy_helper {
    size_t pos;
    abstract_tuple* target;
    tuple_view_copy_helper(abstract_tuple* trgt) : pos(0), target(trgt) { }
    template<typename T>
    void operator()(const T* value) {
        *(reinterpret_cast<T*>(target->mutable_at(pos++))) = *value;
    }
};

template<typename... Ts>
class tuple_view : public abstract_tuple {

    static_assert(sizeof...(Ts) > 0,
                  "tuple_vals is not allowed to be empty");

    typedef abstract_tuple super;

 public:

    typedef tdata<util::rebindable_reference<Ts>...> data_type;

    typedef types_array<Ts...> element_types;

    tuple_view() = delete;
    tuple_view(const tuple_view&) = delete;

    /**
     * @warning @p tuple_view does @b NOT takes ownership for given pointers
     */
    tuple_view(Ts*... args) : super(false), m_data(args...) { }

    inline data_type& data() {
        return m_data;
    }

    inline const data_type& data() const {
        return m_data;
    }

    size_t size() const {
        return sizeof...(Ts);
    }

    abstract_tuple* copy() const {
        return new tuple_vals<Ts...>{m_data};
    }

    const void* at(size_t pos) const {
        CPPA_REQUIRE(pos < size());
        return m_data.at(pos).get_ptr();
    }

    void* mutable_at(size_t pos) {
        CPPA_REQUIRE(pos < size());
        return m_data.mutable_at(pos).get_ptr();
    }

    const uniform_type_info* type_at(size_t pos) const {
        CPPA_REQUIRE(pos < size());
        return m_types[pos];
    }

    const std::type_info* type_token() const {
        return detail::static_type_list<Ts...>::list;
    }

 private:

    data_type m_data;

    static types_array<Ts...> m_types;

    tuple_view(const data_type& data) : m_data(data) { }

};

template<typename... Ts>
types_array<Ts...> tuple_view<Ts...>::m_types;

} } // namespace cppa::detail

#endif // CPPA_TUPLE_VIEW_HPP
