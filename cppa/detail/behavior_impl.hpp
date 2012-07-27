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


#ifndef BEHAVIOR_IMPL_HPP
#define BEHAVIOR_IMPL_HPP

#include "cppa/ref_counted.hpp"
#include "cppa/util/duration.hpp"

namespace cppa { namespace detail {

class behavior_impl : public ref_counted {

 public:

    behavior_impl() = default;

    inline behavior_impl(util::duration tout) : m_timeout(tout) { }

    virtual bool invoke(any_tuple&) = 0;
    virtual bool invoke(const any_tuple&) = 0;
    inline bool invoke(any_tuple&& arg) {
        any_tuple tmp(std::move(arg));
        return invoke(tmp);
    }
    virtual bool defined_at(const any_tuple&) = 0;
    virtual void handle_timeout();
    inline const util::duration& timeout() const {
        return m_timeout;
    }

 private:

    util::duration m_timeout;

};

struct dummy_match_expr {
    inline bool invoke(const any_tuple&) { return false; }
    inline bool can_invoke(const any_tuple&) { return false; }
};

template<class MatchExpr, typename F>
class default_behavior_impl : public behavior_impl {

    typedef behavior_impl super;

 public:

    template<typename Expr>
    default_behavior_impl(Expr&& expr, const timeout_definition<F>& d)
    : super(d.timeout), m_expr(std::forward<Expr>(expr)), m_fun(d.handler) { }

    template<typename Expr>
    default_behavior_impl(Expr&& expr, util::duration tout, F f)
    : super(tout), m_expr(std::forward<Expr>(expr)), m_fun(f) { }

    bool invoke(any_tuple& tup) {
        return m_expr.invoke(tup);
    }

    bool invoke(const any_tuple& tup) {
        return m_expr.invoke(tup);
    }

    bool defined_at(const any_tuple& tup) {
        return m_expr.can_invoke(tup);
    }

    void handle_timeout() { m_fun(); }

 private:

   MatchExpr m_expr;
   F m_fun;

};

template<class MatchExpr, typename F>
default_behavior_impl<MatchExpr, F>* new_default_behavior_impl(const MatchExpr& mexpr, util::duration d, F f) {
    return new default_behavior_impl<MatchExpr, F>(mexpr, d, f);
}

} } // namespace cppa::detail

#endif // BEHAVIOR_IMPL_HPP
