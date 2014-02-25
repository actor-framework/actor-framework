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


#ifndef TYPED_BEHAVIOR_HPP
#define TYPED_BEHAVIOR_HPP

#include "cppa/behavior.hpp"
#include "cppa/match_expr.hpp"

#include "cppa/detail/typed_actor_util.hpp"

namespace cppa {

template<typename... Signatures>
class typed_actor;

template<typename... Signatures>
class typed_behavior {

    template<typename... OtherSignatures>
    friend class typed_actor;

    typed_behavior() = delete;

 public:

    typed_behavior(typed_behavior&&) = default;
    typed_behavior(const typed_behavior&) = default;
    typed_behavior& operator=(typed_behavior&&) = default;
    typed_behavior& operator=(const typed_behavior&) = default;

    typedef util::type_list<Signatures...> signatures;

    template<typename... Cs>
    typed_behavior(match_expr<Cs...> expr) {
        set(std::move(expr));
    }

    template<typename... Cs>
    typed_behavior& operator=(match_expr<Cs...> expr) {
        set(std::move(expr));
        return *this;
    }

 private:

    behavior& unbox() { return m_bhvr; }

    template<typename... Cs>
    void set(match_expr<Cs...>&& expr) {
        m_bhvr = std::move(expr);
        using input = util::type_list<
                          typename detail::deduce_signature<Cs>::type...
                      >;
        // check whether the signature is an exact match
        static_assert(util::tl_equal<signatures, input>::value,
                      "'expr' does not match given signature");
    }

    behavior m_bhvr;

};

} // namespace cppa

#endif // TYPED_BEHAVIOR_HPP
