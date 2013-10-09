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


#ifndef OPTIONAL_VARIANT_HPP
#define OPTIONAL_VARIANT_HPP

#include <iosfwd>
#include <stdexcept>
#include <type_traits>

#include "cppa/none.hpp"
#include "cppa/unit.hpp"
#include "cppa/optional.hpp"
#include "cppa/match_hint.hpp"

#include "cppa/util/type_list.hpp"
#include "cppa/util/type_traits.hpp"

#include "cppa/detail/optional_variant_data.hpp"

#define CPPA_OPTIONAL_VARIANT_CASE(x)                                          \
    case x: return do_visit(from, visitor,                                     \
                            make_bool_token<void_pos == x >(),                 \
                            make_int_token< x >())                             \

namespace cppa {

template<int Value>
constexpr std::integral_constant<int, Value> make_int_token() { return {}; }

template<bool Value>
constexpr std::integral_constant<bool, Value> make_bool_token() { return {}; }

template<typename T>
struct optional_variant_copy_helper {
    T& lhs;
    optional_variant_copy_helper(T& lhs_ref) : lhs(lhs_ref) { }
    template<typename U>
    inline void operator()(const U& rhs) const {
        lhs = rhs;
    }
    inline void operator()() const {
        lhs = unit;
    }
};

template<typename T>
struct optional_variant_move_helper {
    T& lhs;
    optional_variant_move_helper(T& lhs_ref) : lhs(lhs_ref) { }
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
class optional_variant;

template<typename T>
struct is_optional_variant {
    static constexpr bool value = false;
};

template<typename... Ts>
struct is_optional_variant<optional_variant<Ts...>> {
    static constexpr bool value = true;
};

/**
 * @brief A optional_variant is either invalid or holds
          a value of one of the types <tt>Ts</tt>.
 */
template<typename... Ts>
class optional_variant {

 public:

    typedef util::type_list<Ts...> types;

    static constexpr int void_pos = util::tl_find<types, void>::value;

    static constexpr bool has_match_hint = util::tl_find<types, match_hint>::value != -1;

    /**
     * @brief Checks whether this objects holds a value of type @p T.
     */
    template<typename T>
    inline bool is() const {
        return m_type != -1 && m_type == util::tl_find<types, T>::value;
    }

    template<typename U>
    optional_variant& operator=(U&& arg) {
        destroy_data();
        set(std::forward<U>(arg));
        return *this;
    }

    optional_variant& operator=(const optional_variant& other) {
        destroy_data();
        optional_variant_copy_helper<optional_variant> helper{*this};
        other.apply(helper);
        return *this;
    }

    optional_variant& operator=(optional_variant&& other) {
        destroy_data();
        optional_variant_move_helper<optional_variant> helper{*this};
        other.apply(helper);
        return *this;
    }

    optional_variant() : m_type(-1) { }

    template<typename U>
    optional_variant(U&& arg) {
        set(std::forward<U>(arg));
    }

    optional_variant(const optional_variant& other) : m_type(-1) {
        optional_variant_copy_helper<optional_variant> helper{*this};
        other.apply(helper);
    }

    optional_variant(optional_variant&& other) : m_type(-1) {
        optional_variant_move_helper<optional_variant> helper{*this};
        other.apply(helper);
    }

    ~optional_variant() {
        destroy_data();
    }

    /**
     * @brief Checks whether this optional_variant is valid.
     */
    inline explicit operator bool() const {
        return m_type != -1;
    }

    /**
     * @brief Checks whether this optional_variant is invalid.
     */
    inline bool operator!() const {
        return m_type == -1;
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
            case -1: return visitor(none_t{});
            CPPA_OPTIONAL_VARIANT_CASE(0);
            CPPA_OPTIONAL_VARIANT_CASE(1);
            CPPA_OPTIONAL_VARIANT_CASE(2);
            CPPA_OPTIONAL_VARIANT_CASE(3);
            CPPA_OPTIONAL_VARIANT_CASE(4);
            CPPA_OPTIONAL_VARIANT_CASE(5);
            CPPA_OPTIONAL_VARIANT_CASE(6);
            CPPA_OPTIONAL_VARIANT_CASE(7);
            CPPA_OPTIONAL_VARIANT_CASE(8);
            CPPA_OPTIONAL_VARIANT_CASE(9);
        }
    }

    inline void destroy_data() {
        detail::optional_variant_data_destructor f;
        apply(f);
    }

    template<typename U>
    typename std::enable_if<
           !std::is_same<typename util::rm_const_and_ref<U>::type, none_t>::value
        && !is_optional_variant<typename util::rm_const_and_ref<U>::type>::value
        && !is_optional<typename util::rm_const_and_ref<U>::type>::value
    >::type
    set(U&& arg) {
        typedef typename util::rm_const_and_ref<U>::type stripped_type;
        typedef typename detail::unlift_void<stripped_type>::type type;
        static constexpr int type_id = util::tl_find<types, type>::value;
        static_assert(type_id != -1, "invalid type");
        m_type = type_id;
        auto& ref = m_data.get(make_int_token<type_id>());
        new (&ref) stripped_type (std::forward<U>(arg));
    }

    inline void set(const optional_variant& other) {
        optional_variant_copy_helper<optional_variant> helper{*this};
        other.apply(helper);
    }

    inline void set(optional_variant&& other) {
        optional_variant_move_helper<optional_variant> helper{*this};
        other.apply(helper);
    }

    template<typename T>
    inline void set(const optional<T>& arg) {
        if (arg) set(*arg);
        else set(none);
    }

    template<typename T>
    inline void set(optional<T>&& arg) {
        if (arg) set(std::move(*arg));
        else set(none);
    }

    inline void set(const none_t&) { m_type = -1; }

    int m_type;
    detail::optional_variant_data<typename detail::lift_void<Ts>::type...> m_data;

};

/**
 * @relates optional_variant
 */
template<typename T, typename... Us>
const T& get(const optional_variant<Us...>& value) {
    std::integral_constant<int, util::tl_find<util::type_list<Us...>, T>::value> token;
    return value.get(token);
}

/**
 * @relates optional_variant
 */
template<typename T, typename... Us>
T& get_ref(optional_variant<Us...>& value) {
    std::integral_constant<int, util::tl_find<util::type_list<Us...>, T>::value> token;
    return value.get(token);
}

/**
 * @relates optional_variant
 */
template<typename Visitor, typename... Ts>
auto apply_visitor(Visitor& visitor, const optional_variant<Ts...>& data) -> decltype(visitor(none_t{})) {
    return data.apply(visitor);
}

/**
 * @relates optional_variant
 */
template<typename Visitor, typename... Ts>
auto apply_visitor(const Visitor& visitor, const optional_variant<Ts...>& data) -> decltype(visitor(none_t{})) {
    return data.apply(visitor);
}

/**
 * @relates optional_variant
 */
template<typename Visitor, typename... Ts>
auto apply_visitor(Visitor& visitor, optional_variant<Ts...>& data) -> decltype(visitor(none_t{})) {
    return data.apply(visitor);
}

/**
 * @relates optional_variant
 */
template<typename Visitor, typename... Ts>
auto apply_visitor(const Visitor& visitor, optional_variant<Ts...>& data) -> decltype(visitor(none_t{})) {
    return data.apply(visitor);
}

template<typename T>
struct optional_variant_from_type_list;

template<typename... Ts>
struct optional_variant_from_type_list<util::type_list<Ts...>> {
    typedef optional_variant<Ts...> type;
};

namespace detail {
template<typename T>
struct optional_variant_cmp_helper {
    const T& lhs;
    optional_variant_cmp_helper(const T& value) : lhs{value} { }
    // variant is comparable
    template<typename U>
    typename std::enable_if<util::is_comparable<T, U>::value, bool>::type
    operator()(const U& rhs) const { return lhs == rhs; }
    // variant is not comparable
    template<typename U>
    typename std::enable_if<!util::is_comparable<T, U>::value, bool>::type
    operator()(const U&) const { return false; }
    // variant is void
    bool operator()() const { return false; }
    // variant is undefined
    bool operator()(const none_t&) const { return false; }
};
} // namespace detail

/**
 * @relates optional_variant
 */
template<typename T, typename... Ts>
bool operator==(const T& lhs, const optional_variant<Ts...>& rhs) {
    return apply_visitor(detail::optional_variant_cmp_helper<T>{lhs}, rhs);
}

/**
 * @relates optional_variant
 */
template<typename T, typename... Ts>
bool operator==(const optional_variant<Ts...>& lhs, const T& rhs) {
    return rhs == lhs;
}

namespace detail {
struct optional_variant_ostream_helper {
    std::ostream& lhs;
    inline optional_variant_ostream_helper(std::ostream& os) : lhs(os) { }
    template<typename T>
    std::ostream& operator()(const T& value) const {
        return lhs << value;
    }
    inline std::ostream& operator()(const none_t&) const {
        return lhs << "<none>";
    }
    inline std::ostream& operator()() const {
        return lhs << "<void>";
    }
    template<typename T0>
    std::ostream& operator()(const cow_tuple<T0>& value) const {
        return lhs << get<0>(value);
    }
    template<typename T0, typename T1, typename... Ts>
    std::ostream& operator()(const cow_tuple<T0, T1, Ts...>& value) const {
        lhs << get<0>(value) << " ";
        return (*this)(value.drop_left());
    }
};
} // namespace detail

/**
 * @relates optional_variant
 */
template<typename T0, typename... Ts>
std::ostream& operator<<(std::ostream& lhs, const optional_variant<T0, Ts...>& rhs) {
    return apply_visitor(detail::optional_variant_ostream_helper{lhs}, rhs);
}

template<typename T, typename... Ts>
optional_variant<T, typename util::rm_const_and_ref<Ts>::type...>
make_optional_variant(T value, Ts&&... args) {
    return {std::move(value), std::forward<Ts>(args)...};
}

template<typename... Ts>
inline optional_variant<Ts...> make_optional_variant(optional_variant<Ts...> value) {
    return std::move(value);
}

} // namespace cppa

#endif // OPTIONAL_VARIANT_HPP
