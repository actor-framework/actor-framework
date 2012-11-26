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


#include <cstdint>
#include <stdexcept>
#include "cppa/util/fiber.hpp"

#ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace cppa { namespace util {

fiber::fiber() : m_impl(nullptr) { }

fiber::fiber(void (*)(void*), void*) : m_impl(nullptr) { }

fiber::~fiber() { }

void fiber::swap(fiber&, fiber&) {
    throw std::logic_error("libcppa was compiled using "
                           "CPPA_DISABLE_CONTEXT_SWITCHING");
}

} } // namespace cppa::util

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

#ifndef NVALGRIND
#include <valgrind/valgrind.h>
#endif
#include <boost/version.hpp>
#include <boost/context/all.hpp>

namespace cppa { namespace util {

void fiber_trampoline(intptr_t iptr);

#if BOOST_VERSION == 105100
namespace ctx = boost::ctx;
#else
namespace ctx = boost::context;
#endif

class fiber_impl {

 public:

    fiber_impl() : m_ctx{} { }

    virtual ~fiber_impl() { }

    virtual void run() { }

    void swap(fiber_impl* to) {
#if BOOST_VERSION == 105100
        ctx::jump_fcontext(&m_ctx, &to->m_ctx, (intptr_t) to);
#else
        ctx::jump_fcontext(m_ctx, to->m_ctx, (intptr_t) to);
#endif
    }

 protected:

#if BOOST_VERSION == 105100
    ctx::fcontext_t m_ctx;
#else
    ctx::fcontext_t* m_ctx;
#endif

};

// a fiber representing a thread ('converts' the thread to a fiber)
class converted_fiber : public fiber_impl {

 public:

#if BOOST_VERSION == 105100
    converted_fiber() { m_ctx = m_ctx_obj; }
#else
    converted_fiber() { m_ctx = &m_ctx_obj; }
#endif

 private:

    ctx::fcontext_t m_ctx_obj;

};

// a fiber executing a function
class fun_fiber : public fiber_impl {

#if BOOST_VERSION == 105100
    typedef ctx::stack_allocator allocator;
#else
    typedef ctx::guarded_stack_allocator allocator;
#endif

 public:

    fun_fiber(void (*fun)(void*), void* arg) : m_arg(arg), m_fun(fun) {
#if BOOST_VERSION == 105100
        size_t stacksize = ctx::minimum_stacksize();
        m_ctx.fc_stack.base = m_alloc.allocate(stacksize);
        m_ctx.fc_stack.limit = reinterpret_cast<void *>(
                reinterpret_cast<intptr_t>(m_ctx.fc_stack.base)-stacksize);
        ctx::make_fcontext(&m_ctx, fiber_trampoline);
#ifndef NVALGRIND
        m_valgrind_stack_id =
            VALGRIND_STACK_REGISTER(m_ctx.fc_stack.base,
                    reinterpret_cast<intptr_t>(m_ctx.fc_stack.base)-stacksize);
#endif
#else // BOOST_VERSION
        size_t stacksize = allocator::minimum_stacksize();
        m_ctx = ctx::make_fcontext(m_alloc.allocate(stacksize), stacksize, fiber_trampoline);
#ifndef NVALGRIND
        m_valgrind_stack_id =
            VALGRIND_STACK_REGISTER(m_ctx.fc_stack.sp,
                    reinterpret_cast<intptr_t>(m_ctx.fc_stack.sp)-stacksize);
#endif
#endif // BOOST_VERSION
    }

    ~fun_fiber() {
#ifndef NVALGRIND
            VALGRIND_STACK_DEREGISTER(m_valgrind_stack_id);
#endif

#if BOOST_VERSION == 105100
        size_t stacksize =
            reinterpret_cast<intptr_t>(m_ctx.fc_stack.base)
            - reinterpret_cast<intptr_t>(m_ctx.fc_stack.base);
        m_alloc.deallocate(m_ctx.fc_stack.base, stacksize);
#else
        m_alloc.deallocate(m_ctx->fc_stack.sp, m_ctx->fc_stack.size);
#endif
    }

    virtual void run() {
        m_fun(m_arg);
    }


 private:

    void* m_arg;
    void (*m_fun)(void*);
    allocator m_alloc;
#ifndef NVALGRIND
    //! stack id so valgrind doesn't freak when stack swapping happens
    int m_valgrind_stack_id;
#endif


};

void fiber_trampoline(intptr_t iptr) {
    auto ptr = (fiber_impl*) iptr;
    ptr->run();
}

fiber::fiber() : m_impl(new converted_fiber) { }

fiber::fiber(void (*f)(void*), void* arg) : m_impl(new fun_fiber(f, arg)) { }

void fiber::swap(fiber& from, fiber& to) {
    from.m_impl->swap(to.m_impl);
}

fiber::~fiber() {
    delete m_impl;
}

} } // namespace cppa::util

#endif // CPPA_DISABLE_CONTEXT_SWITCHING
