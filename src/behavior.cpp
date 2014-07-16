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

#include "caf/none.hpp"

#include "caf/behavior.hpp"
#include "caf/message_handler.hpp"

namespace caf {

namespace {
class continuation_decorator : public detail::behavior_impl {

    using super = behavior_impl;

 public:

    using continuation_fun = behavior::continuation_fun;

    using pointer = typename behavior_impl::pointer;

    continuation_decorator(continuation_fun fun, pointer ptr)
            : super(ptr->timeout()), m_fun(fun), m_decorated(std::move(ptr)) {
        CAF_REQUIRE(m_decorated != nullptr);
    }

    template<typename T>
    inline bhvr_invoke_result invoke_impl(T& tup) {
        auto res = m_decorated->invoke(tup);
        if (res) return m_fun(*res);
        return none;
    }

    bhvr_invoke_result invoke(message& tup) { return invoke_impl(tup); }

    bhvr_invoke_result invoke(const message& tup) { return invoke_impl(tup); }

    pointer copy(const generic_timeout_definition& tdef) const {
        return new continuation_decorator(m_fun, m_decorated->copy(tdef));
    }

    void handle_timeout() { m_decorated->handle_timeout(); }

 private:

    continuation_fun m_fun;
    pointer m_decorated;

};
} // namespace <anonymous>

behavior::behavior(const message_handler& fun) : m_impl(fun.m_impl) {}

behavior behavior::add_continuation(continuation_fun fun) {
    return behavior::impl_ptr{
        new continuation_decorator(std::move(fun), m_impl)};
}

} // namespace caf
