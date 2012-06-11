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


#ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace { int keep_compiler_happy() { return 42; } }

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

#include <cstdint>
#include <boost/context/all.hpp>

#include "cppa/util/fiber.hpp"

namespace cppa { namespace util {

void fiber_trampoline(intptr_t iptr);

struct fiber_impl {

    void* m_arg;
    bool m_converted;
    void (*m_fun)(void*);
    boost::ctx::fcontext_t m_ctx;
    boost::ctx::stack_allocator m_alloc;

    fiber_impl() : m_arg(nullptr), m_converted(true), m_fun(nullptr) { }

    fiber_impl(void (*fun)(void*), void* arg) : m_arg(arg), m_converted(false), m_fun(fun) {
        auto st = m_alloc.allocate(boost::ctx::minimum_stacksize());
        m_ctx.fc_stack.base = st;
        m_ctx.fc_stack.limit = static_cast<char*>(st) - boost::ctx::minimum_stacksize();
        boost::ctx::make_fcontext(&m_ctx, fiber_trampoline);
    }

    inline void run() {
        m_fun(m_arg);
    }

    void swap(fiber_impl* to) {
        boost::ctx::jump_fcontext(&m_ctx, &(to->m_ctx), (intptr_t) to);
    }

    ~fiber_impl() {
        if (m_converted == false) {
            m_alloc.deallocate(m_ctx.fc_stack.base, boost::ctx::minimum_stacksize());
        }
    }

};

void fiber_trampoline(intptr_t iptr) {
    auto ptr = (fiber_impl*) iptr;
    ptr->run();
}

fiber::fiber() throw() : m_impl(new fiber_impl) {
}

fiber::fiber(void (*func)(void*), void* arg)
: m_impl(new fiber_impl(func, arg)) {
}

void fiber::swap(fiber& from, fiber& to) {
    from.m_impl->swap(to.m_impl);
}

fiber::~fiber() {
    delete m_impl;
}

} } // namespace cppa::util

#endif // CPPA_DISABLE_CONTEXT_SWITCHING
