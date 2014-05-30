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


#ifndef CPPA_DETAIL_VALUE_GUARD_HPP
#define CPPA_DETAIL_VALUE_GUARD_HPP

#include <type_traits>

#include "cppa/unit.hpp"

#include "cppa/util/algorithm.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/type_traits.hpp"

#include "cppa/detail/tdata.hpp"

namespace cppa {
namespace detail {

// 'absorbs' callables and instances of `anything`
template<typename T>
const T& vg_fwd(const T& arg, typename std::enable_if<not util::is_callable<T>::value>::type* = 0) {
    return arg;
}

inline unit_t vg_fwd(const anything&) {
    return unit;
}

template<typename T>
unit_t vg_fwd(const T&, typename std::enable_if<util::is_callable<T>::value>::type* = 0) {
    return unit;
}

template<typename T>
struct vg_cmp {
    template<typename U>
    inline static bool _(const T& lhs, const U& rhs) {
        return util::safe_equal(lhs, rhs);
    }
};

template<>
struct vg_cmp<unit_t> {
    template<typename T>
    inline static bool _(const unit_t&, const T&) {
        return true;
    }
};

template<typename FilteredPattern>
class value_guard {

 public:

    value_guard() = default;
    value_guard(const value_guard&) = default;

    template<typename... Ts>
    value_guard(const Ts&... args) : m_args(vg_fwd(args)...) { }

    template<typename... Ts>
    inline bool operator()(const Ts&... args) const {
        return _eval(m_args.head, m_args.tail(), args...);
    }

 private:

    typename tdata_from_type_list<FilteredPattern>::type m_args;

    template<typename T, typename U>
    static inline bool cmp(const T& lhs, const U& rhs) {
        return vg_cmp<T>::_(lhs, rhs);
    }

    template<typename T, typename U>
    static inline bool cmp(const T& lhs, const std::reference_wrapper<U>& rhs) {
        return vg_cmp<T>::_(lhs, rhs.get());
    }

    static inline bool _eval(const unit_t&, const tdata<>&) {
        return true;
    }

    template<typename T, typename U, typename... Us>
    static inline bool _eval(const T& head, const tdata<>&,
                             const U& arg, const Us&...) {
        return cmp(head, arg);
    }

    template<typename T0, typename T1, typename... Ts,
             typename U, typename... Us>
    static inline bool _eval(const T0& head, const tdata<T1, Ts...>& tail,
                             const U& arg, const Us&... args) {
        return cmp(head, arg) && _eval(tail.head, tail.tail(), args...);
    }

};

typedef value_guard<util::empty_type_list> empty_value_guard;

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_VALUE_GUARD_HPP
