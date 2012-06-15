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


#ifndef CPPA_VALUE_GUARD_HPP
#define CPPA_VALUE_GUARD_HPP

#include <type_traits>

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/void_type.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/callable_trait.hpp"

#include "cppa/detail/tdata.hpp"

namespace cppa { namespace detail {

template<bool IsFun, typename T>
struct vg_fwd_ {
    static inline const T& _(const T& arg) { return arg; }
    static inline T&& _(T&& arg) { return std::move(arg); }
    static inline T& _(T& arg) { return arg; }
};

template<typename T>
struct vg_fwd_<true, T> {
    template<typename Arg>
    static inline util::void_type _(Arg&&) { return {}; }
};

// absorbs callables
template<typename T>
struct vg_fwd
        : vg_fwd_<util::is_callable<typename util::rm_ref<T>::type>::value,
                  typename util::rm_ref<T>::type> {
};

template<typename T>
struct vg_cmp {
    template<typename U>
    inline static bool _(const T& lhs, const U& rhs) {
        return lhs == rhs;
    }
};

template<>
struct vg_cmp<util::void_type> {
    template<typename T>
    inline static bool _(const util::void_type&, const T&) {
        return true;
    }
};

template<typename FilteredPattern>
class value_guard {

 public:

    value_guard() = default;
    value_guard(const value_guard&) = default;

    template<typename... Args>
    value_guard(const Args&... args) : m_args(vg_fwd<Args>::_(args)...) {
    }

    template<typename... Args>
    inline bool operator()(const Args&... args) const {
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

    static inline bool _eval(const util::void_type&, const tdata<>&) {
        return true;
    }

    template<typename Head, typename Arg0, typename... Args>
    static inline bool _eval(const Head& head, const tdata<>&,
                             const Arg0& arg0, const Args&...) {
        return cmp(head, arg0);
    }

    template<typename Head,
             typename Tail0, typename... Tail,
             typename Arg0, typename... Args>
    static inline bool _eval(const Head& head, const tdata<Tail0,Tail...>& tail,
                             const Arg0& arg0, const Args&... args) {
        return cmp(head, arg0) && _eval(tail.head, tail.tail(), args...);
    }

};

} } // namespace cppa::detail

#endif // CPPA_VALUE_GUARD_HPP
