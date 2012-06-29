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


#ifndef CPPA_COW_TUPLE_HPP
#define CPPA_COW_TUPLE_HPP

#include <cstddef>
#include <string>
#include <typeinfo>
#include <type_traits>

#include "cppa/get.hpp"
#include "cppa/actor.hpp"
#include "cppa/cow_ptr.hpp"
#include "cppa/ref_counted.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/util/is_comparable.hpp"
#include "cppa/util/compare_tuples.hpp"
#include "cppa/util/is_legal_tuple_type.hpp"

#include "cppa/detail/tuple_vals.hpp"
#include "cppa/detail/decorated_tuple.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {

// forward declaration
class any_tuple;
class local_actor;

/**
 * @ingroup CopyOnWrite
 * @brief A fixed-length copy-on-write cow_tuple.
 */
template<typename... ElementTypes>
class cow_tuple {

    static_assert(sizeof...(ElementTypes) > 0, "tuple is empty");

    static_assert(util::tl_forall<util::type_list<ElementTypes...>,
                                  util::is_legal_tuple_type>::value,
                  "illegal types in cow_tuple definition: "
                  "pointers and references are prohibited");

    friend class any_tuple;

    typedef detail::tuple_vals<ElementTypes...> data_type;
    typedef detail::decorated_tuple<ElementTypes...> decorated_type;

    cow_ptr<detail::abstract_tuple> m_vals;

    struct priv_ctor { };

    cow_tuple(priv_ctor, cow_ptr<detail::abstract_tuple>&& ptr) : m_vals(std::move(ptr)) { }

 public:

    typedef util::type_list<ElementTypes...> types;
    typedef cow_ptr<detail::abstract_tuple> cow_ptr_type;

    static constexpr size_t num_elements = sizeof...(ElementTypes);

    /**
     * @brief Initializes each element with its default constructor.
     */
    cow_tuple() : m_vals(new data_type) {
    }

    /**
     * @brief Initializes the cow_tuple with @p args.
     * @param args Initialization values.
     */
    cow_tuple(const ElementTypes&... args) : m_vals(new data_type(args...)) {
    }

    /**
     * @brief Initializes the cow_tuple with @p args.
     * @param args Initialization values.
     */
    cow_tuple(ElementTypes&&... args) : m_vals(new data_type(std::move(args)...)) {
    }

    cow_tuple(cow_tuple&&) = default;
    cow_tuple(const cow_tuple&) = default;
    cow_tuple& operator=(cow_tuple&&) = default;
    cow_tuple& operator=(const cow_tuple&) = default;

    inline static cow_tuple from(cow_ptr_type ptr) {
        return {priv_ctor{}, std::move(ptr)};
    }

    inline static cow_tuple from(cow_ptr_type ptr,
                             const util::fixed_vector<size_t, num_elements>& mv) {
        return {priv_ctor{}, decorated_type::create(std::move(ptr), mv)};
    }

    inline static cow_tuple offset_subtuple(cow_ptr_type ptr, size_t offset) {
        CPPA_REQUIRE(offset > 0);
        return {priv_ctor{}, decorated_type::create(std::move(ptr), offset)};
    }

    /**
     * @brief Gets the size of this cow_tuple.
     */
    inline size_t size() const {
        return sizeof...(ElementTypes);
    }

    /**
     * @brief Gets a const pointer to the element at position @p p.
     */
    inline const void* at(size_t p) const {
        return m_vals->at(p);
    }

    /**
     * @brief Gets a mutable pointer to the element at position @p p.
     */
    inline void* mutable_at(size_t p) {
        return m_vals->mutable_at(p);
    }

    /**
     * @brief Gets {@link uniform_type_info uniform type information}
     *        of the element at position @p p.
     */
    inline const uniform_type_info* type_at(size_t p) const {
        return m_vals->type_at(p);
    }

    inline const cow_ptr<detail::abstract_tuple>& vals() const {
        return m_vals;
    }

};

template<typename TypeList>
struct cow_tuple_from_type_list;

template<typename... Types>
struct cow_tuple_from_type_list< util::type_list<Types...> > {
    typedef cow_tuple<Types...> type;
};

#ifdef CPPA_DOCUMENTATION

/**
 * @ingroup CopyOnWrite
 * @brief Gets a const-reference to the <tt>N</tt>th element of @p tup.
 * @param tup The cow_tuple object.
 * @returns A const-reference of type T, whereas T is the type of the
 *          <tt>N</tt>th element of @p tup.
 * @relates cow_tuple
 */
template<size_t N, typename T>
const T& get(const cow_tuple<...>& tup);

/**
 * @ingroup CopyOnWrite
 * @brief Gets a reference to the <tt>N</tt>th element of @p tup.
 * @param tup The cow_tuple object.
 * @returns A reference of type T, whereas T is the type of the
 *          <tt>N</tt>th element of @p tup.
 * @note Detaches @p tup if there are two or more references to the cow_tuple data.
 * @relates cow_tuple
 */
template<size_t N, typename T>
T& get_ref(cow_tuple<...>& tup);

/**
 * @ingroup ImplicitConversion
 * @brief Creates a new cow_tuple from @p args.
 * @param args Values for the cow_tuple elements.
 * @returns A cow_tuple object containing the values @p args.
 * @relates cow_tuple
 */
template<typename... Args>
cow_tuple<Args...> make_cow_tuple(Args&&... args);

#else

template<size_t N, typename... Types>
const typename util::at<N, Types...>::type& get(const cow_tuple<Types...>& tup) {
    typedef typename util::at<N, Types...>::type result_type;
    return *reinterpret_cast<const result_type*>(tup.at(N));
}

template<size_t N, typename... Types>
typename util::at<N, Types...>::type& get_ref(cow_tuple<Types...>& tup) {
    typedef typename util::at<N, Types...>::type result_type;
    return *reinterpret_cast<result_type*>(tup.mutable_at(N));
}

template<typename... Args>
cow_tuple<typename detail::strip_and_convert<Args>::type...>
make_cow_tuple(Args&&... args) {
    return {std::forward<Args>(args)...};
}

#endif

/**
 * @brief Compares two cow_tuples.
 * @param lhs First cow_tuple object.
 * @param rhs Second cow_tuple object.
 * @returns @p true if @p lhs and @p rhs are equal; otherwise @p false.
 * @relates cow_tuple
 */
template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(const cow_tuple<LhsTypes...>& lhs,
                       const cow_tuple<RhsTypes...>& rhs) {
    return util::compare_tuples(lhs, rhs);
}

/**
 * @brief Compares two cow_tuples.
 * @param lhs First cow_tuple object.
 * @param rhs Second cow_tuple object.
 * @returns @p true if @p lhs and @p rhs are not equal; otherwise @p false.
 * @relates cow_tuple
 */
template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(const cow_tuple<LhsTypes...>& lhs,
                       const cow_tuple<RhsTypes...>& rhs) {
    return !(lhs == rhs);
}

} // namespace cppa

#endif // CPPA_COW_TUPLE_HPP
