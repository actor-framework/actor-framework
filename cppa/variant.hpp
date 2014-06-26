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

#include "cppa/lift_void.hpp"

#include "cppa/util/type_list.hpp"
#include "cppa/util/type_traits.hpp"

#include "cppa/detail/variant_data.hpp"

#define CPPA_VARIANT_CASE(x)                                                   \
    case x: return do_visit(from, visitor,                                     \
                            std::integral_constant<bool, void_pos == x >(),    \
                            std::integral_constant<int,  x >())                \


namespace cppa {

template<typename T>
struct variant_copy_helper {
    T& lhs;
    variant_copy_helper(T& lhs_ref) : lhs(lhs_ref) { }
    template<typename U>
    inline void operator()(const U& rhs) const {
        lhs = rhs;
    }
    inline void operator()() const {
        lhs = unit;
    }
    inline void operator()(const none_t&) const {
        lhs = none;
    }
};

template<typename T>
struct variant_move_helper {
    T& lhs;
    variant_move_helper(T& lhs_ref) : lhs(lhs_ref) { }
    template<typename U>
    inline void operator()(U& rhs) const {
        lhs = std::move(rhs);
    }
    inline void operator()() const {
        lhs = unit;
    }
    inline void operator()(const none_t&) const {
        lhs = none;
    }
};

template<typename... Ts>
class variant;

template<typename T>
struct is_variant {
    static constexpr bool value = false;
};

template<typename... Ts>
struct is_variant<variant<Ts...>> {
    static constexpr bool value = true;
};

/**
 * @brief A variant represents always a valid value of one of
 *        the types @p Ts.
 */
template<typename... Ts>
class variant {

 public:

    typedef util::type_list<Ts...> types;

    static_assert(!util::tl_exists<types, std::is_reference>::value,
                  "Cannot create a variant of references");

    static constexpr int void_pos = util::tl_find<types, void>::value;

    /**
     * @brief Checks whether this objects holds a value of type @p T.
     */
    template<typename T>
    inline bool is() const {
        return m_type == util::tl_find<types, T>::value;
    }

    template<int Pos>
    inline bool is(std::integral_constant<int, Pos>) const {
        return m_type == Pos;
    }

    template<typename U>
    variant& operator=(U&& arg) {
        destroy_data();
        set(std::forward<U>(arg));
        return *this;
    }

    variant& operator=(const variant& other) {
        destroy_data();
        variant_copy_helper<variant> helper{*this};
        other.apply(helper);
        return *this;
    }

    variant& operator=(variant&& other) {
        destroy_data();
        variant_move_helper<variant> helper{*this};
        other.apply(helper);
        return *this;
    }

    variant() : m_type(0) { }

    template<typename U>
    variant(U&& arg) {
        set(std::forward<U>(arg));
    }

    variant(const variant& other) : m_type(0) {
        variant_copy_helper<variant> helper{*this};
        other.apply(helper);
    }

    variant(variant&& other) : m_type(0) {
        variant_move_helper<variant> helper{*this};
        other.apply(helper);
    }

    ~variant() {
        destroy_data();
    }

    /** @cond PRIVATE */

    template<int Pos>
    inline const typename util::tl_at<types, Pos>::type&
    get(std::integral_constant<int, Pos> token,
        typename std::enable_if<Pos < sizeof...(Ts) && Pos != void_pos>::type* = nullptr) const {
        return m_data.get(token);
    }

    template<int Pos>
    inline void get(std::integral_constant<int, Pos>,
                    typename std::enable_if<Pos >= sizeof...(Ts) || Pos == void_pos>::type* = nullptr) const { }

    template<int Pos>
    inline typename util::tl_at<types, Pos>::type&
    get(std::integral_constant<int, Pos> token,
        typename std::enable_if<Pos < sizeof...(Ts) && Pos != void_pos>::type* = nullptr) {
        return m_data.get(token);
    }

    template<typename Visitor>
    auto apply(Visitor& visitor) const -> decltype(visitor(none_t{})) {
        return do_apply(*this, visitor);
    }

    template<typename Visitor>
    auto apply(Visitor& visitor) -> decltype(visitor(none_t{})) {
        return do_apply(*this, visitor);
    }

    /** @endcond */

 private:

    template<typename Self, typename Visitor, int Pos>
    static auto do_visit(Self& from,
                         const Visitor& visitor,
                         std::integral_constant<bool, false> /* is_not_void */,
                         std::integral_constant<int, Pos> token,
                         typename std::enable_if<Pos < sizeof...(Ts)>::type* = nullptr)
    -> decltype(visitor(none_t{})) {
        return visitor(from.get(token));
    }

    template<typename Self, typename Visitor, int Pos>
    static auto do_visit(Self&,
                         const Visitor& visitor,
                         std::integral_constant<bool, true> /* is_void */,
                         std::integral_constant<int, Pos>,
                         typename std::enable_if<Pos < sizeof...(Ts)>::type* = nullptr)
    -> decltype(visitor(none_t{})) {
        return visitor();
    }

    template<typename Self, typename Visitor, int Pos>
    static auto do_visit(Self&,
                         const Visitor& visitor,
                         std::integral_constant<bool, false>,
                         std::integral_constant<int, Pos>,
                         typename std::enable_if<Pos >= sizeof...(Ts)>::type* = nullptr)
    -> decltype(visitor(none_t{})) {
        // this is not the function you are looking for
        throw std::runtime_error("invalid type found");
    }

    template<typename Self, typename Visitor>
    static auto do_apply(Self& from, Visitor& visitor) -> decltype(visitor(none_t{})) {
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
        }
    }

    inline void destroy_data() {
        detail::variant_data_destructor f;
        apply(f);
    }

    template<typename U>
    typename std::enable_if<
           !is_variant<typename util::rm_const_and_ref<U>::type>::value
    >::type
    set(U&& arg) {
        using type = typename util::rm_const_and_ref<U>::type;
        static constexpr int type_id = util::tl_find<types, type>::value;
        m_type = type_id;
        auto& ref = m_data.get(std::integral_constant<int, type_id>());
        new (&ref) type (std::forward<U>(arg));
    }

    inline void set(const variant& other) {
        variant_copy_helper<variant> helper{*this};
        other.apply(helper);
    }

    inline void set(variant&& other) {
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
const T& get(const variant<Us...>& value) {
    std::integral_constant<int, util::tl_find<util::type_list<Us...>, T>::value> token;
    return value.get(token);
}

/**
 * @relates variant
 */
template<typename T, typename... Us>
T& get(variant<Us...>& value) {
    std::integral_constant<int, util::tl_find<util::type_list<Us...>, T>::value> token;
    return value.get(token);
}

/**
 * @relates variant
 */
template<typename T, typename... Us>
const T* get(const variant<Us...>* value) {
    std::integral_constant<int, util::tl_find<util::type_list<Us...>, T>::value> token;
    return value && value->is(token) ? &value->get(token) : nullptr;
}

/**
 * @relates variant
 */
template<typename T, typename... Us>
T* get(variant<Us...>* value) {
    std::integral_constant<int, util::tl_find<util::type_list<Us...>, T>::value> token;
    return value && value->is(token) ? &value->get(token) : nullptr;
}

template<typename Result>
struct static_visitor {
    using result_type = Result;
};

/**
 * @relates variant
 */
template<typename Visitor, typename... Ts>
typename Visitor::result_type apply_visitor(const Visitor& visitor,
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

/**
 * @relates variant
 */
template<typename Visitor, typename... Ts>
typename Visitor::result_type apply_visitor(const Visitor& visitor,
                                            variant<Ts...>& data) {
    return data.apply(visitor);
}

} // namespace cppa

#endif // CPPA_VARIANT_HPP
