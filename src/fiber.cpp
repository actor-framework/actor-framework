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

bool fiber::is_disabled_feature() {
    return true;
}

} } // namespace cppa::util

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

#ifdef CPPA_ANNOTATE_VALGRIND
#include <valgrind/valgrind.h>
#endif
#include <boost/version.hpp>
#include <boost/context/all.hpp>
#if BOOST_VERSION >= 105300
#include <boost/coroutine/all.hpp>
#endif

namespace cppa { namespace util {

struct fiber_impl;
void fiber_trampoline(intptr_t iptr);

namespace {

#if CPPA_ANNOTATE_VALGRIND
typedef int vg_member;
inline void vg_register(vg_member& stack_id, void* ptr1, void* ptr2) {
    stack_id = VALGRIND_STACK_REGISTER(ptr1, ptr2);
}
inline void vg_deregister(vg_member stack_id) {
    VALGRIND_STACK_DEREGISTER(stack_id);
}
#else
struct vg_member { };
template<typename... Ts> inline void vg_register(const Ts&...) { }
inline void vg_deregister(const vg_member&) { }
#endif

#if BOOST_VERSION == 105100
namespace ctx = boost::ctx;
typedef ctx::fcontext_t fc_member;
typedef ctx::stack_allocator fc_allocator;
inline void fc_jump(fc_member& from, fc_member& to, fiber_impl* ptr) {
    ctx::jump_fcontext(&from, &to, (intptr_t) ptr);
}
inline void fc_make(fc_member& storage, fc_allocator& alloc, vg_member& vgm) {
    size_t mss = ctx::minimum_stacksize();
    storage.fc_stack.base = alloc.allocate(mss);
    storage.fc_stack.limit = reinterpret_cast<void*>(
                reinterpret_cast<intptr_t>(storage.fc_stack.base) - mss);
    ctx::make_fcontext(&storage, fiber_trampoline);
    vg_register(vgm,
                storage.fc_stack.base,
                reinterpret_cast<void*>(
                    reinterpret_cast<intptr_t>(storage.fc_stack.base) - mss));
}
#else
namespace ctx = boost::context;
typedef ctx::fcontext_t* fc_member;
#   if BOOST_VERSION < 105300
    typedef ctx::guarded_stack_allocator fc_allocator;
#   else
    typedef boost::coroutines::stack_allocator fc_allocator;
#   endif
inline void fc_jump(fc_member& from, fc_member& to, fiber_impl* ptr) {
    ctx::jump_fcontext(from, to, (intptr_t) ptr);
}
inline void fc_make(fc_member& storage, fc_allocator& alloc, vg_member& vgm) {
    size_t mss = fc_allocator::minimum_stacksize();
    storage = ctx::make_fcontext(alloc.allocate(mss), mss, fiber_trampoline);
    vg_register(vgm,
                storage->fc_stack.sp,
                reinterpret_cast<void*>(
                    reinterpret_cast<intptr_t>(storage->fc_stack.sp) - mss));
}
#endif

} // namespace <anonymous>

struct fiber_impl {

    fiber_impl() : m_ctx() { }

    virtual ~fiber_impl() { }

    virtual void run() { }

    void swap(fiber_impl* to) {
        fc_jump(m_ctx, to->m_ctx, to);
    }

    fc_member m_ctx;

};

// a fiber representing a thread ('converts' the thread to a fiber)
struct converted_fiber : fiber_impl {

    converted_fiber() {
#       if BOOST_VERSION > 105100
        m_ctx = &m_ctx_obj;
#       endif
    }

#   if BOOST_VERSION > 105100
    ctx::fcontext_t m_ctx_obj;
#   endif

};

// a fiber executing a function
struct fun_fiber : fiber_impl {

    fun_fiber(void (*fun)(void*), void* arg) : m_arg(arg), m_fun(fun) {
        fc_make(m_ctx, m_alloc, m_vgm);
    }

    ~fun_fiber() {
        vg_deregister(m_vgm);
    }

    virtual void run() {
        m_fun(m_arg);
    }

    void* m_arg;
    void (*m_fun)(void*);
    fc_allocator m_alloc;
    vg_member m_vgm;

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

bool fiber::is_disabled_feature() {
    return false;
}

} } // namespace cppa::util

#endif // CPPA_DISABLE_CONTEXT_SWITCHING
