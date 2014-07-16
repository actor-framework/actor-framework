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

#ifndef CAF_DETAIL_VARIANT_DATA_HPP
#define CAF_DETAIL_VARIANT_DATA_HPP

#include <stdexcept>
#include <type_traits>

#include "caf/unit.hpp"
#include "caf/none.hpp"

#define CAF_VARIANT_DATA_CONCAT(x, y) x ## y

#define CAF_VARIANT_DATA_GETTER(pos)                                          \
    inline CAF_VARIANT_DATA_CONCAT(T, pos) &                                  \
    get(std::integral_constant<int, pos >) {                                   \
        return CAF_VARIANT_DATA_CONCAT(v, pos) ;                              \
    }                                                                          \
    inline const CAF_VARIANT_DATA_CONCAT(T, pos) &                            \
    get(std::integral_constant<int, pos >) const {                             \
        return CAF_VARIANT_DATA_CONCAT(v, pos) ;                              \
    }

namespace caf {
namespace detail {

template<typename T0,           typename T1  = unit_t, typename T2  = unit_t,
         typename T3  = unit_t, typename T4  = unit_t, typename T5  = unit_t,
         typename T6  = unit_t, typename T7  = unit_t, typename T8  = unit_t,
         typename T9  = unit_t, typename T10 = unit_t, typename T11 = unit_t,
         typename T12 = unit_t, typename T13 = unit_t, typename T14 = unit_t,
         typename T15 = unit_t, typename T16 = unit_t, typename T17 = unit_t,
         typename T18 = unit_t, typename T19 = unit_t, typename T20 = unit_t>
struct variant_data {

    union {
        T0  v0;  T1  v1;  T2  v2;
        T3  v3;  T4  v4;  T5  v5;
        T6  v6;  T7  v7;  T8  v8;
        T9  v9;  T10 v10; T11 v11;
        T12 v12; T13 v13; T14 v14;
        T11 v15; T12 v16; T13 v17;
        T14 v18; T15 v19; T20 v20;
    };

    variant_data() { }

    ~variant_data() { }

    CAF_VARIANT_DATA_GETTER(0)
    CAF_VARIANT_DATA_GETTER(1)
    CAF_VARIANT_DATA_GETTER(2)
    CAF_VARIANT_DATA_GETTER(3)
    CAF_VARIANT_DATA_GETTER(4)
    CAF_VARIANT_DATA_GETTER(5)
    CAF_VARIANT_DATA_GETTER(6)
    CAF_VARIANT_DATA_GETTER(7)
    CAF_VARIANT_DATA_GETTER(8)
    CAF_VARIANT_DATA_GETTER(9)
    CAF_VARIANT_DATA_GETTER(10)
    CAF_VARIANT_DATA_GETTER(11)
    CAF_VARIANT_DATA_GETTER(12)
    CAF_VARIANT_DATA_GETTER(13)
    CAF_VARIANT_DATA_GETTER(14)
    CAF_VARIANT_DATA_GETTER(15)
    CAF_VARIANT_DATA_GETTER(16)
    CAF_VARIANT_DATA_GETTER(17)
    CAF_VARIANT_DATA_GETTER(18)
    CAF_VARIANT_DATA_GETTER(19)
    CAF_VARIANT_DATA_GETTER(20)

 private:

    template<typename M, typename U>
    inline void cr(M& member, U&& arg) {
        new (&member) M (std::forward<U>(arg));
    }

};

struct variant_data_destructor {
    using result_type = void;
    template<typename T>
    inline void operator()(T& storage) const { storage.~T(); }
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_VARIANT_DATA_HPP
