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


#ifndef BEHAVIOR_IMPL_HPP
#define BEHAVIOR_IMPL_HPP

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/timeout_definition.hpp"

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

    typedef intrusive_ptr<behavior_impl> pointer;

    virtual pointer copy(const generic_timeout_definition& tdef) const = 0;

    inline pointer or_else(const pointer& other) {
        CPPA_REQUIRE(other != nullptr);
        struct combinator : behavior_impl {
            pointer first;
            pointer second;
            bool invoke(any_tuple& arg) {
                return first->invoke(arg) || second->invoke(arg);
            }
            bool invoke(const any_tuple& arg) {
                return first->invoke(arg) || second->invoke(arg);
            }
            bool defined_at(const any_tuple& arg) {
                return first->defined_at(arg) || second->defined_at(arg);
            }
            void handle_timeout() {
                // the second behavior overrides the timeout handling of
                // first behavior
                return second->handle_timeout();
            }
            pointer copy(const generic_timeout_definition& tdef) const {
                return new combinator(first, second->copy(tdef));
            }
            combinator(const pointer& p0, const pointer& p1)
            : behavior_impl(p1->timeout()), first(p0), second(p1) { }
        };
        return new combinator(this, other);
    }

 private:

    util::duration m_timeout;

};

struct dummy_match_expr {
    inline optional_variant<void> invoke(const any_tuple&) const { return none; }
    inline bool can_invoke(const any_tuple&) const { return false; }
    inline optional_variant<void> operator()(const any_tuple&) const { return none; }
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
        return eval_res(m_expr(tup));
    }

    bool invoke(const any_tuple& tup) {
        return eval_res(m_expr(tup));
    }

    bool defined_at(const any_tuple& tup) {
        return m_expr.can_invoke(tup);
    }

    typename behavior_impl::pointer copy(const generic_timeout_definition& tdef) const {
        return new default_behavior_impl<MatchExpr, std::function<void()> >(m_expr, tdef);
    }

    void handle_timeout() { m_fun(); }

 private:

    template<typename T>
    typename std::enable_if<T::has_match_hint == true, bool>::type
    eval_res(const T& res) {
        if (res) {
            if (res.template is<match_hint>()) {
                return get<match_hint>(res) == match_hint::handle;
            }
            return true;
        }
        return false;
    }

    template<typename T>
    typename std::enable_if<T::has_match_hint == false, bool>::type
    eval_res(const T& res) {
        return static_cast<bool>(res);
    }

    MatchExpr m_expr;
    F m_fun;

};

template<class MatchExpr, typename F>
default_behavior_impl<MatchExpr, F>* new_default_behavior(const MatchExpr& mexpr, util::duration d, F f) {
    return new default_behavior_impl<MatchExpr, F>(mexpr, d, f);
}

template<typename F>
default_behavior_impl<dummy_match_expr, F>* new_default_behavior(util::duration d, F f) {
    return new default_behavior_impl<dummy_match_expr, F>(dummy_match_expr{}, d, f);
}

template<typename F>
class continuation_decorator : public behavior_impl {

    typedef behavior_impl super;

 public:

    typedef typename behavior_impl::pointer pointer;

    template<typename Fun>
    continuation_decorator(Fun&& fun, pointer decorated)
    : super(decorated->timeout()), m_fun(std::forward<Fun>(fun))
    , m_decorated(std::move(decorated)) {
        CPPA_REQUIRE(m_decorated != nullptr);
    }

    template<typename T>
    inline bool invoke_impl(T& tup) {
        if (m_decorated->invoke(tup)) {
            m_fun();
            return true;
        }
        return false;
    }

    bool invoke(any_tuple& tup) {
        return invoke_impl(tup);
    }

    bool invoke(const any_tuple& tup) {
        return invoke_impl(tup);
    }

    bool defined_at(const any_tuple& tup) {
        return m_decorated->defined_at(tup);
    }

    pointer copy(const generic_timeout_definition& tdef) const {
        return new continuation_decorator<F>(m_fun, m_decorated->copy(tdef));
    }

    void handle_timeout() { m_decorated->handle_timeout(); }

 private:

    F m_fun;
    pointer m_decorated;

};

typedef intrusive_ptr<behavior_impl> behavior_impl_ptr;

} } // namespace cppa::detail

#endif // BEHAVIOR_IMPL_HPP
