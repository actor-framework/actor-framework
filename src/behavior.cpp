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


#include "cppa/behavior.hpp"
#include "cppa/partial_function.hpp"

namespace cppa {

class continuation_decorator : public detail::behavior_impl {

    typedef behavior_impl super;

 public:

    typedef behavior::continuation_fun continuation_fun;

    typedef typename behavior_impl::pointer pointer;

    ~continuation_decorator();

    continuation_decorator(continuation_fun fun, pointer ptr)
    : super(ptr->timeout()), m_fun(fun), m_decorated(std::move(ptr)) {
        CPPA_REQUIRE(m_decorated != nullptr);
    }

    template<typename T>
    inline bhvr_invoke_result invoke_impl(T& tup) {
        auto res = m_decorated->invoke(tup);
        if (res) return m_fun(*res);
        return none;
    }

    bhvr_invoke_result invoke(any_tuple& tup) {
        return invoke_impl(tup);
    }

    bhvr_invoke_result invoke(const any_tuple& tup) {
        return invoke_impl(tup);
    }

    bool defined_at(const any_tuple& tup) {
        return m_decorated->defined_at(tup);
    }

    pointer copy(const generic_timeout_definition& tdef) const {
        return new continuation_decorator(m_fun, m_decorated->copy(tdef));
    }

    void handle_timeout() { m_decorated->handle_timeout(); }

 private:

    continuation_fun m_fun;
    pointer m_decorated;

};

// avoid weak-vtables warning by providing dtor out-of-line
continuation_decorator::~continuation_decorator() { }

behavior::behavior(const partial_function& fun) : m_impl(fun.m_impl) { }

behavior behavior::add_continuation(continuation_fun fun) {
    return behavior::impl_ptr{new continuation_decorator(std::move(fun),
                                                         m_impl)};
}

} // namespace cppa
