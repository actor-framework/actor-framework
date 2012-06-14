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

#include <type_traits>

#include "cppa/config.hpp"
#include "cppa/cow_ptr.hpp"
#include "cppa/cow_tuple.hpp"

#include "cppa/util/rm_ref.hpp"
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

    explicit any_tuple(detail::abstract_tuple*);

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
    inline bool empty() const { return size() == 0; }

    template<typename T>
    inline const T& get_as(size_t p) const {
        CPPA_REQUIRE(*(type_at(p)) == typeid(T));
        return *reinterpret_cast<const T*>(at(p));
    }

    template<typename T>
    inline T& get_as_mutable(size_t p) {
        CPPA_REQUIRE(*(type_at(p)) == typeid(T));
        return *reinterpret_cast<T*>(mutable_at(p));
    }

    inline const_iterator begin() const { return m_vals->begin(); }

    inline const_iterator end() const { return m_vals->end(); }

    inline cow_ptr<detail::abstract_tuple>& vals()  { return m_vals; }
    inline const cow_ptr<detail::abstract_tuple>& vals() const { return m_vals; }
    inline const cow_ptr<detail::abstract_tuple>& cvals() const { return m_vals; }

    inline const std::type_info* type_token() const {
        return m_vals->type_token();
    }

    inline detail::tuple_impl_info impl_type() const {
        return m_vals->impl_type();
    }

    template<typename T>
    static inline any_tuple view(T&& value,
                                 typename std::enable_if<
                                     util::is_iterable<
                                         typename util::rm_ref<T>::type
                                     >::value
                                 >::type* = 0) {
        static constexpr bool can_optimize =    std::is_reference<T>::value
                                             && !std::is_const<T>::value;
        std::integral_constant<bool, can_optimize> token;
        return any_tuple{container_view(std::forward<T>(value), token)};
    }

    template<typename T>
    static inline any_tuple view(T&& value,
                                 typename std::enable_if<
                                     util::is_iterable<
                                         typename util::rm_ref<T>::type
                                     >::value == false
                                 >::type* = 0) {
        typedef typename util::rm_ref<T>::type vtype;
        typedef typename detail::implicit_conversions<vtype>::type converted;
        static_assert(util::is_legal_tuple_type<converted>::value,
                      "T is not a valid tuple type");
        static constexpr bool can_optimize =
                   std::is_same<converted, vtype>::value
                && std::is_reference<T>::value
                && !std::is_const<typename std::remove_reference<T>::type>::value;
        std::integral_constant<bool, can_optimize> token;
        return any_tuple{simple_view(std::forward<T>(value), token)};
    }

    void force_detach() {
        m_vals.detach();
    }

    void reset();

 private:

    cow_ptr<detail::abstract_tuple> m_vals;

    explicit any_tuple(const cow_ptr<detail::abstract_tuple>& vals);

    typedef detail::abstract_tuple* tup_ptr;

    template<typename T>
    static inline tup_ptr simple_view(T& value,
                                      std::integral_constant<bool, true>) {
        return new detail::tuple_view<T>(&value);
    }

    template<typename First, typename Second>
    static inline tup_ptr simple_view(std::pair<First, Second>& p,
                                      std::integral_constant<bool, true>) {
        return new detail::tuple_view<First, Second>(&p.first, &p.second);
    }

    template<typename T>
    static inline tup_ptr simple_view(T&& value,
                                      std::integral_constant<bool, false>) {
        typedef typename util::rm_ref<T>::type vtype;
        typedef typename detail::implicit_conversions<vtype>::type converted;
        return new detail::tuple_vals<converted>(std::forward<T>(value));
    }

    template<typename First, typename Second>
    static inline any_tuple view(std::pair<First, Second> p,
                                 std::integral_constant<bool, false>) {
       return new detail::tuple_vals<First, Second>(std::move(p.first),
                                                    std::move(p.second));
    }

    template<typename T>
    static inline detail::abstract_tuple* container_view(T& value,
                                                 std::integral_constant<bool, true>) {
        return new detail::container_tuple_view<T>(&value);
    }

    template<typename T>
    static inline detail::abstract_tuple* container_view(T&& value,
                                                 std::integral_constant<bool, false>) {
        typedef typename util::rm_ref<T>::type ctype;
        return new detail::container_tuple_view<T>(new ctype(std::forward<T>(value)), true);
    }

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

} // namespace cppa

#endif // ANY_TUPLE_HPP
