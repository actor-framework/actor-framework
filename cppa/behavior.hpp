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


#ifndef CPPA_BEHAVIOR_HPP
#define CPPA_BEHAVIOR_HPP

#include <functional>
#include <type_traits>

#include "cppa/match_expr.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/util/tbind.hpp"
#include "cppa/util/rm_ref.hpp"
#include "cppa/util/if_else.hpp"
#include "cppa/util/duration.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/disjunction.hpp"

namespace cppa {

/**
 * @brief Describes the behavior of an actor.
 */
class behavior : public partial_function {

    typedef partial_function super;

 public:

    behavior() = default;
    behavior(behavior&&) = default;
    behavior(const behavior&) = default;
    behavior& operator=(behavior&&) = default;
    behavior& operator=(const behavior&) = default;

    inline behavior(partial_function fun) : super(std::move(fun)) { }

    inline behavior(partial_function::impl_ptr ptr) : super(std::move(ptr)) { }

    template<typename F>
    behavior(const timeout_definition<F>& arg)
    : super(new default_behavior_impl<dummy_match_expr, F>(dummy_match_expr{},
                                                           arg.timeout,
                                                           arg.handler)) {
    }

    template<typename F>
    behavior(util::duration d, F f)
    : super(new default_behavior_impl<dummy_match_expr, F>(dummy_match_expr{},
                                                           d, f)) {
    }

    template<typename... Cases>
    behavior(const match_expr<Cases...>& arg0)
    : super(arg0) { }

    template<typename... Cases, typename Arg1, typename... Args>
    behavior(const match_expr<Cases...>& arg0, const Arg1& arg1, const Args&... args)
    : super(match_expr_concat(arg0, arg1, args...)) { }

    inline void handle_timeout() {
        m_impl->handle_timeout();
    }

    inline const util::duration& timeout() const {
        return m_impl->timeout();
    }

    inline bool undefined() const {
        return m_impl == nullptr && m_impl->timeout().valid();
    }

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

#endif // CPPA_BEHAVIOR_HPP
