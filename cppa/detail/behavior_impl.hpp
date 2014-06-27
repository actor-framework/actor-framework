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

#ifndef BEHAVIOR_IMPL_HPP
#define BEHAVIOR_IMPL_HPP

#include <tuple>
#include <type_traits>

#include "cppa/none.hpp"
#include "cppa/variant.hpp"
#include "cppa/optional.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/atom.hpp"
#include "cppa/message.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/skip_message.hpp"
#include "cppa/timeout_definition.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/int_list.hpp"
#include "cppa/util/type_traits.hpp"

#include "cppa/detail/apply_args.hpp"

namespace cppa {

class message_handler;
typedef optional<message> bhvr_invoke_result;

} // namespace cppa

namespace cppa {
namespace detail {

template<class T>
struct is_message_id_wrapper {
    template<class U>
    static char (&test(typename U::message_id_wrapper_tag))[1];
    template<class U>
    static char (&test(...))[2];
    static constexpr bool value = sizeof(test<T>(0)) == 1;
};

struct optional_message_visitor : static_visitor<bhvr_invoke_result> {
    // inline bhvr_invoke_result operator()() const { return message{}; }
    inline bhvr_invoke_result operator()(none_t&) const { return none; }
    inline bhvr_invoke_result operator()(const none_t&) const { return none; }
    inline bhvr_invoke_result operator()(skip_message_t&) const { return none; }
    inline bhvr_invoke_result operator()(const skip_message_t&) const {
        return none;
    }
    inline bhvr_invoke_result operator()(unit_t&) const {
        return message{};
    }
    inline bhvr_invoke_result operator()(const unit_t&) const {
        return message{};
    }
    inline bhvr_invoke_result operator()(optional<skip_message_t>& val) const {
        if (val) return none;
        return message{};
    }
    inline bhvr_invoke_result
    operator()(const optional<skip_message_t>& val) const {
        if (val) return none;
        return message{};
    }
    template<typename T, typename... Ts>
    inline typename std::enable_if<not is_message_id_wrapper<T>::value,
                                   bhvr_invoke_result>::type
    operator()(T& v, Ts&... vs) const {
        return make_message(std::move(v), std::move(vs)...);
    }
    template<typename T>
    inline bhvr_invoke_result
    operator()(T& value,
               typename std::enable_if<is_message_id_wrapper<T>::value>::type* =
                   0) const {
        return make_message(atom("MESSAGE_ID"),
                            value.get_message_id().integer_value());
    }
    inline bhvr_invoke_result operator()(message& value) const {
        return std::move(value);
    }
    template<typename... Ts>
    inline bhvr_invoke_result operator()(std::tuple<Ts...>& value) const {
        return detail::apply_args(*this, util::get_indices(value), value);
    }
};

template<typename... Ts>
struct has_skip_message {
    static constexpr bool value =
        util::disjunction<std::is_same<Ts, skip_message_t>::value...>::value;
};

class behavior_impl : public ref_counted {

 public:
    behavior_impl() = default;

    inline behavior_impl(util::duration tout) : m_timeout(tout) {}

    virtual bhvr_invoke_result invoke(message&) = 0;
    virtual bhvr_invoke_result invoke(const message&) = 0;
    inline bhvr_invoke_result invoke(message&& arg) {
        message tmp(std::move(arg));
        return invoke(tmp);
    }
    virtual void handle_timeout();
    inline const util::duration& timeout() const { return m_timeout; }

    typedef intrusive_ptr<behavior_impl> pointer;

    virtual pointer copy(const generic_timeout_definition& tdef) const = 0;

    inline pointer or_else(const pointer& other) {
        CPPA_REQUIRE(other != nullptr);
        struct combinator : behavior_impl {
            pointer first;
            pointer second;
            bhvr_invoke_result invoke(message& arg) {
                auto res = first->invoke(arg);
                if (!res) return second->invoke(arg);
                return res;
            }
            bhvr_invoke_result invoke(const message& arg) {
                auto res = first->invoke(arg);
                if (!res) return second->invoke(arg);
                return res;
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
                    : behavior_impl(p1->timeout()), first(p0), second(p1) {}
        };
        return new combinator(this, other);
    }

 private:

    util::duration m_timeout;

};

struct dummy_match_expr {
    inline variant<none_t> invoke(const message&) const { return none; }
    inline bool can_invoke(const message&) const { return false; }
    inline variant<none_t> operator()(const message&) const { return none; }
};

template<class MatchExpr, typename F>
class default_behavior_impl : public behavior_impl {

    typedef behavior_impl super;

 public:
    template<typename Expr>
    default_behavior_impl(Expr&& expr, const timeout_definition<F>& d)
            : super(d.timeout)
            , m_expr(std::forward<Expr>(expr))
            , m_fun(d.handler) {}

    template<typename Expr>
    default_behavior_impl(Expr&& expr, util::duration tout, F f)
            : super(tout), m_expr(std::forward<Expr>(expr)), m_fun(f) {}

    bhvr_invoke_result invoke(message& tup) {
        auto res = m_expr(tup);
        return apply_visitor(optional_message_visitor{}, res);
    }

    bhvr_invoke_result invoke(const message& tup) {
        auto res = m_expr(tup);
        return apply_visitor(optional_message_visitor{}, res);
    }

    typename behavior_impl::pointer
    copy(const generic_timeout_definition& tdef) const {
        return new default_behavior_impl<MatchExpr, std::function<void()>>(
            m_expr, tdef);
    }

    void handle_timeout() { m_fun(); }

 private:
    MatchExpr m_expr;
    F m_fun;
};

template<class MatchExpr, typename F>
default_behavior_impl<MatchExpr, F>*
new_default_behavior(const MatchExpr& mexpr, util::duration d, F f) {
    return new default_behavior_impl<MatchExpr, F>(mexpr, d, f);
}

template<typename F>
default_behavior_impl<dummy_match_expr, F>* new_default_behavior(util::duration d,
                                                                 F f) {
    return new default_behavior_impl<dummy_match_expr, F>(dummy_match_expr{}, d,
                                                          f);
}

typedef intrusive_ptr<behavior_impl> behavior_impl_ptr;

// implemented in message_handler.cpp
// message_handler combine(behavior_impl_ptr, behavior_impl_ptr);
// behavior_impl_ptr extract(const message_handler&);

} // namespace detail
} // namespace cppa

#endif // BEHAVIOR_IMPL_HPP
