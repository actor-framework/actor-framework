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


#ifndef CPPA_PARTIAL_FUNCTION_HPP
#define CPPA_PARTIAL_FUNCTION_HPP

#include <list>
#include <vector>
#include <memory>
#include <utility>

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

    template<typename F>
    inline behavior or_else(const timeout_definition<F>& tdef) const {
        generic_timeout_definition gtd{tdef.timeout, tdef.handler};
        return behavior{m_impl->copy(gtd)};
    }

    template<typename... Cases>
    inline partial_function or_else(const match_expr<Cases...>& mexpr) const {
        return m_impl->or_else(mexpr.as_behavior_impl());
    }

    template<typename Arg0, typename Arg1, typename... Args>
    typename util::if_else<
            util::disjunction<
                is_timeout_definition<Arg0>,
                is_timeout_definition<Arg1>,
                is_timeout_definition<Args>...>,
            behavior,
            util::wrapped<partial_function> >::type
    or_else(const Arg0& arg0, const Arg1& arg1, const Args&... args) const {
        return m_impl->or_else(match_expr_concat(arg0, arg1, args...));
    }

 private:

    impl_ptr m_impl;

};

template<typename Arg0, typename... Args>
typename util::if_else<
            util::disjunction<
                is_timeout_definition<Arg0>,
                is_timeout_definition<Args>...>,
            behavior,
            util::wrapped<partial_function> >::type
match_expr_convert(const Arg0& arg0, const Args&... args) {
    return {match_expr_concat(arg0, args...)};
}

} // namespace cppa

#endif // CPPA_PARTIAL_FUNCTION_HPP
