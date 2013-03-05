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


#ifndef CPPA_ANY_TUPLE_HPP
#define CPPA_ANY_TUPLE_HPP

#include <type_traits>

#include "cppa/config.hpp"
#include "cppa/cow_ptr.hpp"
#include "cppa/cow_tuple.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/comparable.hpp"
#include "cppa/util/is_iterable.hpp"

#include "cppa/detail/tuple_view.hpp"
#include "cppa/detail/abstract_tuple.hpp"
#include "cppa/detail/container_tuple_view.hpp"
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {

/**
 * @brief Describes a fixed-length copy-on-write tuple
 *        with elements of any type.
 */
class any_tuple {

 public:

    /**
     * @brief A smart pointer to the internal implementation.
     */
    typedef cow_ptr<detail::abstract_tuple> data_ptr;

    /**
     * @brief An iterator to access each element as <tt>const void*</tt>.
     */
    typedef detail::abstract_tuple::const_iterator const_iterator;

    /**
     * @brief Creates an empty tuple.
     */
    any_tuple();

    /**
     * @brief Creates a tuple from @p t.
     */
    template<typename... Args>
    any_tuple(const cow_tuple<Args...>& t) : m_vals(t.vals()) { }

    /**
     * @brief Creates a tuple and moves the content from @p t.
     */
    template<typename... Args>
    any_tuple(cow_tuple<Args...>&& t) : m_vals(std::move(t.m_vals)) { }

    /**
     * @brief Move constructor.
     */
    any_tuple(any_tuple&&);

    /**
     * @brief Copy constructor.
     */
    any_tuple(const any_tuple&) = default;

    /**
     * @brief Move assignment.
     */
    any_tuple& operator=(any_tuple&&);

    /**
     * @brief Copy assignment.
     */
    any_tuple& operator=(const any_tuple&) = default;

    /**
     * @brief Gets the size of this tuple.
     */
    size_t size() const;

    /**
     * @brief Gets a mutable pointer to the element at position @p p.
     */
    void* mutable_at(size_t p);

    /**
     * @brief Gets a const pointer to the element at position @p p.
     */
    const void* at(size_t p) const;

    /**
     * @brief Gets {@link uniform_type_info uniform type information}
     *        of the element at position @p p.
     */
    const uniform_type_info* type_at(size_t p) const;

    /**
     * @brief Returns @c true if <tt>*this == other</tt>, otherwise false.
     */
    bool equals(const any_tuple& other) const;

    /**
     * @brief Returns true if <tt>size() == 0</tt>, otherwise false.
     */
    inline bool empty() const;

    /**
     * @brief Returns the value at @p as instance of @p T.
     */
    template<typename T>
    inline const T& get_as(size_t p) const;

    /**
     * @brief Returns the value at @p as mutable data_ptr& of type @p T&.
     */
    template<typename T>
    inline T& get_as_mutable(size_t p);

    /**
     * @brief Returns an iterator to the beginning.
     */
    inline const_iterator begin() const;

    /**
     * @brief Returns an iterator to the end.
     */
    inline const_iterator end() const;

    /**
     * @brief Returns a copy-on-write pointer to the internal data.
     */
    inline data_ptr& vals();

    /**
     * @brief Returns a const copy-on-write pointer to the internal data.
     */
    inline const data_ptr& vals() const;

    /**
     * @brief Returns a const copy-on-write pointer to the internal data.
     */
    inline const data_ptr& cvals() const;

    /**
     * @brief Returns either <tt>&typeid(util::type_list<Ts...>)</tt>, where
     *        <tt>Ts...</tt> are the element types, or <tt>&typeid(void)</tt>.
     *
     * The type token @p &typeid(void) indicates that this tuple is dynamically
     * typed, i.e., the types where not available at compile time.
     */
    inline const std::type_info* type_token() const;

    /**
     * @brief Checks whether this
     */
    inline bool dynamically_typed() const;

    template<typename T>
    static inline any_tuple view(T&& value);

    inline void force_detach();

    void reset();

    explicit any_tuple(detail::abstract_tuple*);

 private:

    data_ptr m_vals;

    explicit any_tuple(const data_ptr& vals);

    template<typename T, typename CanOptimize>
    static any_tuple view(T&& value, std::true_type, CanOptimize token);

    template<typename T, typename CanOptimize>
    static any_tuple view(T&& value, std::false_type, CanOptimize token);

    template<typename T, typename U>
    static any_tuple view(std::pair<T,U> p, std::false_type);

    template<typename T>
    static detail::abstract_tuple* simple_view(T& value, std::true_type);

    template<typename T, typename U>
    static detail::abstract_tuple* simple_view(std::pair<T,U>& p, std::true_type);

    template<typename T>
    static detail::abstract_tuple* simple_view(T&& value, std::false_type);

    template<typename T>
    static detail::abstract_tuple* container_view(T& value, std::true_type);

    template<typename T>
    static detail::abstract_tuple* container_view(T&& value, std::false_type);

};

/**
 * @relates any_tuple
 */
inline bool operator==(const any_tuple& lhs, const any_tuple& rhs) {
    return lhs.equals(rhs);
}

/**
 * @relates any_tuple
 */
inline bool operator!=(const any_tuple& lhs, const any_tuple& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Creates an {@link any_tuple} containing the elements @p args.
 * @param args Values to initialize the tuple elements.
 */
template<typename... Args>
inline any_tuple make_any_tuple(Args&&... args) {
    return make_cow_tuple(std::forward<Args>(args)...);
}

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline bool any_tuple::empty() const {
    return size() == 0;
}

template<typename T>
inline const T& any_tuple::get_as(size_t p) const {
    CPPA_REQUIRE(*(type_at(p)) == typeid(T));
    return *reinterpret_cast<const T*>(at(p));
}

template<typename T>
inline T& any_tuple::get_as_mutable(size_t p) {
    CPPA_REQUIRE(*(type_at(p)) == typeid(T));
    return *reinterpret_cast<T*>(mutable_at(p));
}

inline any_tuple::const_iterator any_tuple::begin() const {
    return m_vals->begin();
}

inline any_tuple::const_iterator any_tuple::end() const {
    return m_vals->end();
}

inline any_tuple::data_ptr& any_tuple::vals() {
    return m_vals;
}

inline const any_tuple::data_ptr& any_tuple::vals() const {
    return m_vals;
}

inline const any_tuple::data_ptr& any_tuple::cvals() const {
    return m_vals;
}

inline const std::type_info* any_tuple::type_token() const {
    return m_vals->type_token();
}

inline bool any_tuple::dynamically_typed() const {
    return m_vals->dynamically_typed();
}

inline void any_tuple::force_detach() {
    m_vals.detach();
}

template<typename T, bool IsIterable = true>
struct any_tuple_view_trait_impl {
    static constexpr bool is_mutable_ref =    std::is_reference<T>::value
                                           && !std::is_const<T>::value;
    typedef std::integral_constant<bool,is_mutable_ref> can_optimize;
    typedef std::integral_constant<bool,true> is_container;
};

template<typename T>
struct any_tuple_view_trait_impl<T,false> {
    typedef typename util::rm_ref<T>::type type;
    typedef typename detail::implicit_conversions<type>::type mapped;
    static_assert(util::is_legal_tuple_type<mapped>::value,
                  "T is not a valid tuple type");
    static constexpr bool is_mutable_ref =    std::is_same<mapped,type>::value
                                           && std::is_reference<T>::value
                                           && not std::is_const<T>::value;
    typedef std::integral_constant<bool,is_mutable_ref> can_optimize;
    typedef std::integral_constant<bool,false> is_container;
};

template<typename T>
struct any_tuple_view_trait {
    typedef typename util::rm_ref<T>::type type;
    typedef any_tuple_view_trait_impl<T,util::is_iterable<type>::value> impl;
    typedef typename impl::can_optimize can_optimize;
    typedef typename impl::is_container is_container;
};

template<typename T>
inline any_tuple any_tuple::view(T&& value) {
    typedef any_tuple_view_trait<T> trait;
    typename trait::can_optimize can_optimize;
    typename trait::is_container is_container;
    return view(std::forward<T>(value), is_container, can_optimize);
}

template<typename T, typename CanOptimize>
any_tuple any_tuple::view(T&& v, std::true_type, CanOptimize t) {
    return any_tuple{container_view(std::forward<T>(v), t)};
}

template<typename T, typename CanOptimize>
any_tuple any_tuple::view(T&& v, std::false_type, CanOptimize t) {
    return any_tuple{simple_view(std::forward<T>(v), t)};
}

template<typename T>
detail::abstract_tuple* any_tuple::simple_view(T& v, std::true_type) {
    return new detail::tuple_view<T>(&v);
}

template<typename T, typename U>
detail::abstract_tuple* any_tuple::simple_view(std::pair<T,U>& p, std::true_type) {
    return new detail::tuple_view<T,U>(&p.first, &p.second);
}

template<typename T>
detail::abstract_tuple* any_tuple::simple_view(T&& v, std::false_type) {
    typedef typename util::rm_ref<T>::type vtype;
    typedef typename detail::implicit_conversions<vtype>::type converted;
    return new detail::tuple_vals<converted>(std::forward<T>(v));
}

template<typename T, typename U>
any_tuple any_tuple::view(std::pair<T,U> p, std::false_type) {
   return new detail::tuple_vals<T,U>(std::move(p.first), std::move(p.second));
}

template<typename T>
detail::abstract_tuple* any_tuple::container_view(T& v, std::true_type) {
    return new detail::container_tuple_view<T>(&v);
}

template<typename T>
detail::abstract_tuple* any_tuple::container_view(T&& v, std::false_type) {
    auto vptr = new typename util::rm_ref<T>::type(std::forward<T>(v));
    return new detail::container_tuple_view<T>(vptr, true);
}

} // namespace cppa

#endif // CPPA_ANY_TUPLE_HPP
