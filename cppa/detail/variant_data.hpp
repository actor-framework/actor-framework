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


#ifndef CPPA_DETAIL_VARIANT_DATA_HPP
#define CPPA_DETAIL_VARIANT_DATA_HPP

#include <stdexcept>
#include <type_traits>

#include "cppa/unit.hpp"
#include "cppa/none.hpp"
#include "cppa/lift_void.hpp"

#define CPPA_VARIANT_DATA_CONCAT(x, y) x ## y

#define CPPA_VARIANT_DATA_GETTER(pos)                                 \
    inline CPPA_VARIANT_DATA_CONCAT(T, pos) &                         \
    get(std::integral_constant<int, pos >) {                                   \
        return CPPA_VARIANT_DATA_CONCAT(v, pos) ;                     \
    }                                                                          \
    inline const CPPA_VARIANT_DATA_CONCAT(T, pos) &                   \
    get(std::integral_constant<int, pos >) const {                             \
        return CPPA_VARIANT_DATA_CONCAT(v, pos) ;                     \
    }

namespace cppa {
namespace detail {

template<typename T0,          typename T1 = unit_t,
         typename T2 = unit_t, typename T3 = unit_t,
         typename T4 = unit_t, typename T5 = unit_t,
         typename T6 = unit_t, typename T7 = unit_t,
         typename T8 = unit_t, typename T9 = unit_t>
struct variant_data {

    union {
        T0 v0; T1 v1; T2 v2; T3 v3; T4 v4;
        T5 v5; T6 v6; T7 v7; T8 v8; T9 v9;
    };

    variant_data() { }

    ~variant_data() { }

    CPPA_VARIANT_DATA_GETTER(0)
    CPPA_VARIANT_DATA_GETTER(1)
    CPPA_VARIANT_DATA_GETTER(2)
    CPPA_VARIANT_DATA_GETTER(3)
    CPPA_VARIANT_DATA_GETTER(4)
    CPPA_VARIANT_DATA_GETTER(5)
    CPPA_VARIANT_DATA_GETTER(6)
    CPPA_VARIANT_DATA_GETTER(7)
    CPPA_VARIANT_DATA_GETTER(8)
    CPPA_VARIANT_DATA_GETTER(9)

 private:

    template<typename M, typename U>
    inline void cr(M& member, U&& arg) {
        new (&member) M (std::forward<U>(arg));
    }

};

struct variant_data_destructor {
    inline void operator()() const { }
    inline void operator()(const none_t&) const { }
    template<typename T>
    inline void operator()(T& storage) const { storage.~T(); }
};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_VARIANT_DATA_HPP
