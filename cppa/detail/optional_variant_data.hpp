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


#ifndef TRIVARIANT_DATA_HPP
#define TRIVARIANT_DATA_HPP

#include <stdexcept>
#include <type_traits>

#define CPPA_OPTIONAL_VARIANT_DATA_CONCAT(x, y) x ## y

#define CPPA_OPTIONAL_VARIANT_DATA_GETTER(pos)                                 \
    inline CPPA_OPTIONAL_VARIANT_DATA_CONCAT(T, pos) &                         \
    get(std::integral_constant<int, pos >) {                                   \
        return CPPA_OPTIONAL_VARIANT_DATA_CONCAT(v, pos) ;                     \
    }                                                                          \
    inline const CPPA_OPTIONAL_VARIANT_DATA_CONCAT(T, pos) &                   \
    get(std::integral_constant<int, pos >) const {                             \
        return CPPA_OPTIONAL_VARIANT_DATA_CONCAT(v, pos) ;                     \
    }

namespace cppa { struct none_t; }

namespace cppa { namespace detail {

template<typename T>
struct lift_void { typedef  T type; };

template<>
struct lift_void<void> { typedef util::void_type type; };

template<typename T>
struct unlift_void { typedef  T type; };

template<>
struct unlift_void<util::void_type> { typedef void type; };

template<typename T0,                   typename T1 = util::void_type,
         typename T2 = util::void_type, typename T3 = util::void_type,
         typename T4 = util::void_type, typename T5 = util::void_type,
         typename T6 = util::void_type, typename T7 = util::void_type,
         typename T8 = util::void_type, typename T9 = util::void_type>
struct optional_variant_data {

    union {
        T0 v0; T1 v1; T2 v2; T3 v3; T4 v4;
        T5 v5; T6 v6; T7 v7; T8 v8; T9 v9;
    };

    optional_variant_data() { }

    ~optional_variant_data() { }

    CPPA_OPTIONAL_VARIANT_DATA_GETTER(0)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(1)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(2)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(3)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(4)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(5)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(6)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(7)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(8)
    CPPA_OPTIONAL_VARIANT_DATA_GETTER(9)

 private:

    template<typename M, typename U>
    inline void cr(M& member, U&& arg) {
        new (&member) M (std::forward<U>(arg));
    }

};

struct optional_variant_data_destructor {
    inline void operator()() const { }
    inline void operator()(const none_t&) const { }
    template<typename T>
    inline void operator()(T& storage) const { storage.~T(); }
};

} } // namespace cppa::detail

#endif // TRIVARIANT_DATA_HPP
