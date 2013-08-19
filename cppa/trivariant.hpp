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

#include <stdexcept>
#include <type_traits>

#include "cppa/util/type_list.hpp"
#include "cppa/util/void_type.hpp"

namespace cppa {

namespace detail {

template<typename T0,                   typename T1 = util::void_type,
         typename T2 = util::void_type, typename T3 = util::void_type,
         typename T4 = util::void_type, typename T5 = util::void_type,
         typename T6 = util::void_type, typename T7 = util::void_type,
         typename T8 = util::void_type, typename T9 = util::void_type>
struct variant_data {

    union {
        T0 v0; T1 v1; T2 v2; T3 v3; T4 v4;
        T5 v5; T6 v6; T7 v7; T8 v8; T9 v9;
    };

    variant_data() { }

    template<int Id, typename U>
    variant_data(std::integral_constant<int, Id> token, U&& arg) {
        cr(get(token), std::forward<U>(arg));
    }

    ~variant_data() { }

    inline const T0& get(std::integral_constant<int, 0>) const {
        return v0;
    }

    inline const T1& get(std::integral_constant<int, 1>) const {
        return v1;
    }

    inline const T2& get(std::integral_constant<int, 2>) const {
        return v2;
    }

    inline const T3& get(std::integral_constant<int, 3>) const {
        return v3;
    }

    inline const T4& get(std::integral_constant<int, 4>) const {
        return v4;
    }

    inline const T5& get(std::integral_constant<int, 5>) const {
        return v5;
    }

    inline const T6& get(std::integral_constant<int, 6>) const {
        return v6;
    }

    inline const T7& get(std::integral_constant<int, 7>) const {
        return v7;
    }

    inline const T8& get(std::integral_constant<int, 8>) const {
        return v8;
    }

    inline const T9& get(std::integral_constant<int, 9>) const {
        return v9;
    }

    inline T0& get(std::integral_constant<int, 0>) {
        return v0;
    }

    inline T1& get(std::integral_constant<int, 1>) {
        return v1;
    }

    inline T2& get(std::integral_constant<int, 2>) {
        return v2;
    }

    inline T3& get(std::integral_constant<int, 3>) {
        return v3;
    }

    inline T4& get(std::integral_constant<int, 4>) {
        return v4;
    }

    inline T5& get(std::integral_constant<int, 5>) {
        return v5;
    }

    inline T6& get(std::integral_constant<int, 6>) {
        return v6;
    }

    inline T7& get(std::integral_constant<int, 7>) {
        return v7;
    }

    inline T8& get(std::integral_constant<int, 8>) {
        return v8;
    }

    inline T9& get(std::integral_constant<int, 9>) {
        return v9;
    }

 private:

    template<typename M, typename U>
    inline void cr(M& member, U&& arg) {
        new (&member) M (std::forward<U>(arg));
    }

};

struct destructor {
    template<typename T>
    inline void operator()(T& storage) const { storage.~T(); }
};

template<typename T>
struct copy_constructor {
    const T& other;
    copy_constructor(const T& from) : other{from} { }
    void operator()(T& storage) const { new (&storage) T (other); }
    template<typename U>
    void operator()(U&) const { throw std::runtime_error("type mismatch"); }
};

} // namespace detail

template<typename... T>
class trivariant {

 public:

    typedef util::type_list<T...> types;

    inline bool undefined() const { return m_type == -2; }

    inline bool empty() const { return m_type == -1; }

    template<int Pos>
    inline const typename util::tl_at<types, Pos>::type& get(std::integral_constant<int, Pos> token) const {
        return m_data.get(token);
    }

    template<int Pos>
    inline typename util::tl_at<types, Pos>::type& get(std::integral_constant<int, Pos> token) {
        return m_data.get(token);
    }

    template<typename U>
    inline bool is() const {
        return !empty() && m_type == util::tl_find<types, U>::value;
    }

    template<typename U>
    trivariant& operator=(const U& what) {
        apply(detail::destructor{});
        apply(detail::copy_constructor<U>{what});
    }

    void clear() {
        apply(detail::destructor{});
        m_type = -1;
    }

    void invalidate() {
        apply(detail::destructor{});
        m_type = -2;
    }

    trivariant() : m_type(-2) { }

    trivariant(const util::void_type&) : m_type(-1) { }

    template<typename U>
    trivariant(const U& value)
    : m_type{util::tl_find<types, U>::value}
    , m_data{std::integral_constant<int, util::tl_find<types, U>::value>{}, value} {
        static_assert(util::tl_find<types, U>::value >= 0, "invalid type T");
    }

    ~trivariant() {
        apply(detail::destructor{});
    }

    explicit operator bool() const {
        return m_type != -2;
    }

 private:

    template<typename Visitor>
    void apply(const Visitor& visitor) {
        switch (m_type) {
            default: throw std::runtime_error("invalid type found");
            case -2: break;
            case -1: break;
            case  0: visitor(m_data.v0); break;
            case  1: visitor(m_data.v1); break;
            case  2: visitor(m_data.v2); break;
            case  3: visitor(m_data.v3); break;
            case  4: visitor(m_data.v4); break;
            case  5: visitor(m_data.v5); break;
            case  6: visitor(m_data.v6); break;
            case  7: visitor(m_data.v7); break;
            case  8: visitor(m_data.v8); break;
            case  9: visitor(m_data.v9); break;
        }
    }

    int m_type;
    detail::variant_data<T...> m_data;

};

template<typename T, typename... Us>
const T& get(const trivariant<Us...>& value) {
    std::integral_constant<int, util::tl_find<util::type_list<Us...>, T>::value> token;
    return value.get(token);
}

template<typename T, typename... Us>
T& get_ref(trivariant<Us...>& value) {
    std::integral_constant<int, util::tl_find<util::type_list<Us...>, T>::value> token;
    return value.get(token);
}

} // namespace cppa

#endif // OPTIONAL_VARIANT_HPP
