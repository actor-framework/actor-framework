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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_VARIANT_HPP
#define CPPA_VARIANT_HPP

#include "cppa/detail/type_list.hpp"
#include "cppa/detail/type_traits.hpp"
#include "cppa/detail/variant_data.hpp"

#define CPPA_VARIANT_CASE(x)                                                   \
    case x: return visitor(from.get(std::integral_constant<int,                \
                                       x < max_type_id ? x : max_type_id >()))


namespace cppa {

template<typename T>
struct variant_assign_helper {
    using result_type = void;
    T& lhs;
    variant_assign_helper(T& lhs_ref) : lhs(lhs_ref) { }
    template<typename U>
    inline void operator()(const U& rhs) const {
        lhs = rhs;
    }
};

template<typename T>
struct variant_move_helper {
    using result_type = void;
    T& lhs;
    variant_move_helper(T& lhs_ref) : lhs(lhs_ref) { }
    template<typename U>
    inline void operator()(U& rhs) const {
        lhs = std::move(rhs);
    }
};

template<typename T, typename U,
         bool Enable = std::is_integral<T>::value && std::is_integral<U>::value>
struct is_equal_int_type {
    static constexpr bool value =
               sizeof(T) == sizeof(U)
            && std::is_signed<T>::value == std::is_signed<U>::value;
};

template<typename T, typename U>
struct is_equal_int_type<T, U, false> : std::false_type { };

/**
 * @brief Compares @p T to @p U und evaluates to @p true_type if either
 *        <tt>T == U</tt> or if T and U are both integral types of the
 *        same size and signedness. This works around the issue that
 *        <tt>uint8_t != unsigned char</tt> on some compilers.
 */
template<typename T, typename U>
struct is_same_ish : std::conditional<
                         std::is_same<T, U>::value,
                         std::true_type,
                         is_equal_int_type<T, U>
                     >::type { };

/**
 * @brief A variant represents always a valid value of one of
 *        the types @p Ts.
 */
template<typename... Ts>
class variant {

 public:

    using types = detail::type_list<Ts...>;

    static constexpr int max_type_id = sizeof...(Ts) - 1;

    static_assert(!detail::tl_exists<types, std::is_reference>::value,
                  "Cannot create a variant of references");

    variant& operator=(const variant& other) {
        variant_assign_helper<variant> helper{*this};
        other.apply(helper);
        return *this;
    }

    variant& operator=(variant&& other) {
        variant_move_helper<variant> helper{*this};
        other.apply(helper);
        return *this;
    }

    variant() : m_type(0) {
        // never empty
        set(typename detail::tl_head<types>::type());
    }

    template<typename U>
    variant(U&& arg) : m_type(-1) {
        set(std::forward<U>(arg));
    }

    template<typename U>
    variant& operator=(U&& arg) {
        set(std::forward<U>(arg));
        return *this;
    }

    variant(const variant& other) : m_type(-1) {
        variant_assign_helper<variant> helper{*this};
        other.apply(helper);
    }

    variant(variant&& other) : m_type(-1) {
        variant_move_helper<variant> helper{*this};
        other.apply(helper);
    }

    ~variant() {
        destroy_data();
    }

    /** @cond PRIVATE */

    template<int Pos>
    inline bool is(std::integral_constant<int, Pos>) const {
        return m_type == Pos;
    }

    template<int Pos>
    inline const typename detail::tl_at<types, Pos>::type&
    get(std::integral_constant<int, Pos> token) const {
        return m_data.get(token);
    }

    template<int Pos>
    inline typename detail::tl_at<types, Pos>::type&
    get(std::integral_constant<int, Pos> token) {
        return m_data.get(token);
    }

    template<typename Visitor>
    typename Visitor::result_type apply(Visitor& visitor) const {
        return apply_impl(*this, visitor);
    }

    template<typename Visitor>
    typename Visitor::result_type apply(Visitor& visitor) {
        return apply_impl(*this, visitor);
    }

    /** @endcond */

 private:

    template<typename Self, typename Visitor>
    static typename Visitor::result_type  apply_impl(Self& from, Visitor& visitor) {
        switch (from.m_type) {
            default: throw std::runtime_error("invalid type found");
            CPPA_VARIANT_CASE(0);
            CPPA_VARIANT_CASE(1);
            CPPA_VARIANT_CASE(2);
            CPPA_VARIANT_CASE(3);
            CPPA_VARIANT_CASE(4);
            CPPA_VARIANT_CASE(5);
            CPPA_VARIANT_CASE(6);
            CPPA_VARIANT_CASE(7);
            CPPA_VARIANT_CASE(8);
            CPPA_VARIANT_CASE(9);
            CPPA_VARIANT_CASE(10);
            CPPA_VARIANT_CASE(11);
            CPPA_VARIANT_CASE(12);
            CPPA_VARIANT_CASE(13);
            CPPA_VARIANT_CASE(14);
            CPPA_VARIANT_CASE(15);
            CPPA_VARIANT_CASE(16);
            CPPA_VARIANT_CASE(17);
            CPPA_VARIANT_CASE(18);
            CPPA_VARIANT_CASE(19);
            CPPA_VARIANT_CASE(20);
        }
    }

    inline void destroy_data() {
        if (m_type == -1) return; // nothing to do
        detail::variant_data_destructor f;
        apply(f);
    }

    template<typename U>
    void set(U&& arg) {
        using type = typename detail::rm_const_and_ref<U>::type;
        static constexpr int type_id = detail::tl_find_if<
                                           types,
                                           detail::tbind<
                                               is_same_ish, type
                                           >::template type
                                       >::value;
        static_assert(type_id >= 0, "invalid type for variant");
        std::integral_constant<int, type_id> token;
        if (m_type != type_id) {
            destroy_data();
            m_type = type_id;
            auto& ref = m_data.get(token);
            new (&ref) type (std::forward<U>(arg));
        } else {
             m_data.get(token) = std::forward<U>(arg);
        }
    }

    template<typename... Us>
    inline void set(const variant<Us...>& other) {
        using namespace detail;
        static_assert(tl_is_strict_subset<type_list<Us...>, types>::value,
                      "cannot set variant of type A to variant of type B "
                      "unless the element types of A are a strict subset of "
                      "the element types of B");
        variant_assign_helper<variant> helper{*this};
        other.apply(helper);
    }

    template<typename... Us>
    inline void set(variant<Us...>&& other) {
        using namespace detail;
        static_assert(tl_is_strict_subset<type_list<Us...>, types>::value,
                      "cannot set variant of type A to variant of type B "
                      "unless the element types of A are a strict subset of "
                      "the element types of B");
        variant_move_helper<variant> helper{*this};
        other.apply(helper);
    }

    int m_type;
    detail::variant_data<typename lift_void<Ts>::type...> m_data;

};

/**
 * @relates variant
 */
template<typename T, typename... Us>
T& get(variant<Us...>& value) {
    using namespace detail;
    constexpr int type_id = tl_find_if<type_list<Us...>,
                                       tbind<is_same_ish, T>::template type
                            >::value;
    std::integral_constant<int, type_id> token;
    // silence compiler error about "binding to unrelated types" such as
    // 'signed char' to 'char' (which is obvious bullshit)
    return reinterpret_cast<T&>(value.get(token));
}

/**
 * @relates variant
 */
template<typename T, typename... Us>
const T& get(const variant<Us...>& value) {
    // compiler implicitly restores const because of the return type
    return get<T>(const_cast<variant<Us...>&>(value));
}

/**
 * @relates variant
 */
template<typename T, typename... Us>
T* get(variant<Us...>* value) {
    using namespace detail;
    constexpr int type_id = tl_find_if<type_list<Us...>,
                                       tbind<is_same_ish, T>::template type
                            >::value;
    std::integral_constant<int, type_id> token;
    if (value->is(token)) return &get<T>(*value);
    return nullptr;
}

/**
 * @relates variant
 */
template<typename T, typename... Us>
const T* get(const variant<Us...>* value) {
    // compiler implicitly restores const because of the return type
    return get<T>(const_cast<variant<Us...>*>(value));
}

template<typename Result = void>
struct static_visitor {
    using result_type = Result;
};

/**
 * @relates variant
 */
template<typename Visitor, typename... Ts>
typename Visitor::result_type apply_visitor(Visitor& visitor,
                                            const variant<Ts...>& data) {
    return data.apply(visitor);
}

/**
 * @relates variant
 */
template<typename Visitor, typename... Ts>
typename Visitor::result_type apply_visitor(Visitor& visitor,
                                            variant<Ts...>& data) {
    return data.apply(visitor);
}

} // namespace cppa

#endif // CPPA_VARIANT_HPP
