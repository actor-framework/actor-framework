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


#ifndef CPPA_PARTIAL_FUNCTION_HPP
#define CPPA_PARTIAL_FUNCTION_HPP

#include <list>
#include <vector>
#include <memory>
#include <utility>
#include <type_traits>

#include "cppa/behavior.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/timeout_definition.hpp"

#include "cppa/util/duration.hpp"

#include "cppa/detail/behavior_impl.hpp"

namespace cppa {

class behavior;

/**
 * @brief A partial function implementation
 *        for {@link cppa::any_tuple any_tuples}.
 */
class partial_function {

    friend class behavior;

 public:

    typedef intrusive_ptr<detail::behavior_impl> impl_ptr;

    static constexpr bool may_have_timeout = false;

    partial_function() = default;
    partial_function(partial_function&&) = default;
    partial_function(const partial_function&) = default;
    partial_function& operator=(partial_function&&) = default;
    partial_function& operator=(const partial_function&) = default;

    template<typename... Cases>
    partial_function(const match_expr<Cases...>& mexpr)
    : m_impl(mexpr.as_behavior_impl()) { }

    partial_function(impl_ptr ptr);

    inline bool undefined() const {
        return m_impl == nullptr;
    }

    inline bool defined_at(const any_tuple& value) {
        return (m_impl) && m_impl->defined_at(value);
    }

    template<typename T>
    inline bool operator()(T&& arg) {
        return (m_impl) && m_impl->invoke(std::forward<T>(arg));
    }

    inline partial_function or_else(const partial_function& other) const {
        return m_impl->or_else(other.m_impl);
    }

    inline behavior or_else(const behavior& other) const {
        return behavior{m_impl->or_else(other.m_impl)};
    }

    template<typename T>
    typename std::conditional<T::may_have_timeout,behavior,partial_function>::type
    or_else(const T& arg) const;

    template<typename T0, typename T1, typename... Ts>
    typename std::conditional<
        util::disjunction<
            T0::may_have_timeout,
            T1::may_have_timeout,
            Ts::may_have_timeout...
        >::value,
        behavior,
        partial_function
    >::type
    or_else(const T0& arg0, const T1& arg1, const Ts&... args) const;

    inline const impl_ptr& as_behavior_impl() const;

 private:

    impl_ptr m_impl;

};

template<typename T, typename... Ts>
typename std::conditional<
    util::disjunction<
        T::may_have_timeout,
        Ts::may_have_timeout...
    >::value,
    behavior,
    partial_function
>::type
match_expr_convert(const T& arg0, const Ts&... args) {
    return detail::match_expr_concat(arg0, args...);
}

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline const partial_function::impl_ptr& partial_function::as_behavior_impl() const {
    return m_impl;
}

template<typename T>
typename std::conditional<T::may_have_timeout,behavior,partial_function>::type
partial_function::or_else(const T& arg) const {
    return m_impl->or_else(arg.as_behavior_impl());
}

template<typename T0, typename T1, typename... Ts>
typename std::conditional<
    util::disjunction<
        T0::may_have_timeout,
        T1::may_have_timeout,
        Ts::may_have_timeout...
    >::value,
    behavior,
    partial_function
>::type
partial_function::or_else(const T0& arg0, const T1& arg1, const Ts&... args) const {
    return m_impl->or_else(match_expr_convert(arg0, arg1, args...).as_behavior_impl());
}

} // namespace cppa

#endif // CPPA_PARTIAL_FUNCTION_HPP
