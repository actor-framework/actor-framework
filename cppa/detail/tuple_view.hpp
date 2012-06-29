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


#ifndef CPPA_TUPLE_VIEW_HPP
#define CPPA_TUPLE_VIEW_HPP

#include "cppa/util/static_foreach.hpp"

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

template<typename... ElementTypes>
class tuple_view : public abstract_tuple {

    static_assert(sizeof...(ElementTypes) > 0,
                  "tuple_vals is not allowed to be empty");

    typedef abstract_tuple super;

 public:

    typedef tdata<ElementTypes*...> data_type;

    typedef types_array<ElementTypes...> element_types;

    tuple_view() = delete;
    tuple_view(const tuple_view&) = delete;

    /**
     * @warning @p tuple_view does @b NOT takes ownership for given pointers
     */
    tuple_view(ElementTypes*... args)
        : super(tuple_impl_info::statically_typed), m_data(args...) {
    }

    inline data_type& data() {
        return m_data;
    }

    inline const data_type& data() const {
        return m_data;
    }

    size_t size() const {
        return sizeof...(ElementTypes);
    }

    abstract_tuple* copy() const {
        auto result = new tuple_vals<ElementTypes...>;
        tuple_view_copy_helper f{result};
        util::static_foreach<0, sizeof...(ElementTypes)>::_(m_data, f);
        return result;
    }

    const void* at(size_t pos) const {
        CPPA_REQUIRE(pos < size());
        return m_data.at(pos);
    }

    void* mutable_at(size_t pos) {
        CPPA_REQUIRE(pos < size());
        return m_data.mutable_at(pos);
    }

    const uniform_type_info* type_at(size_t pos) const {
        CPPA_REQUIRE(pos < size());
        return m_types[pos];
    }

    const std::type_info* type_token() const {
        return detail::static_type_list<ElementTypes...>::list;
    }

 private:

    data_type m_data;

    static types_array<ElementTypes...> m_types;

};

template<typename... ElementTypes>
types_array<ElementTypes...> tuple_view<ElementTypes...>::m_types;

} } // namespace cppa::detail

#endif // CPPA_TUPLE_VIEW_HPP
