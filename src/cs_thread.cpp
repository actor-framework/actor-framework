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

#include "cppa/config.hpp"

#include "cppa/detail/cs_thread.hpp"

namespace {

typedef void* vptr;
typedef void (*cst_fun)(vptr);

// Boost's coroutine minimal stack size is pretty small
// and easily causes stack overflows when using libcppa
// in debug mode or with logging
#if defined(CPPA_DEBUG_MODE) || defined(CPPA_LOG_LEVEL)
constexpr size_t stack_multiplier = 4;
#else
constexpr size_t stack_multiplier = 2;
#endif

} // namespace <anonmyous>

#if defined(CPPA_DISABLE_CONTEXT_SWITCHING) || defined(CPPA_STANDALONE_BUILD)

namespace cppa { namespace detail {

cs_thread::cs_thread() : m_impl(nullptr) { }

cs_thread::cs_thread(cst_fun, vptr) : m_impl(nullptr) { }

cs_thread::~cs_thread() { }

void cs_thread::swap(cs_thread&, cs_thread&) {
    throw std::logic_error("libcppa was compiled using "
                           "CPPA_DISABLE_CONTEXT_SWITCHING");
}

const bool cs_thread::is_disabled_feature = true;

} } // namespace cppa::detail

#else // CPPA_DISABLE_CONTEXT_SWITCHING  || CPPA_STANDALONE_BUILD

// optional valgrind include
#ifdef CPPA_ANNOTATE_VALGRIND
#   include <valgrind/valgrind.h>
#endif

CPPA_PUSH_WARNINGS
// boost includes
#include <boost/version.hpp>
#include <boost/context/all.hpp>
#include <boost/coroutine/all.hpp>
CPPA_POP_WARNINGS

namespace cppa { namespace detail {

void cst_trampoline(intptr_t iptr);

namespace {

#ifdef CPPA_ANNOTATE_VALGRIND
    typedef int vg_member;
    inline void vg_register(vg_member& stack_id, vptr ptr1, vptr ptr2) {
        stack_id = VALGRIND_STACK_REGISTER(ptr1, ptr2);
    }
    inline void vg_deregister(vg_member stack_id) {
        VALGRIND_STACK_DEREGISTER(stack_id);
    }
#else
    struct vg_member { };
    inline void vg_register(vg_member&, vptr, vptr) {/*NOP*/}
    inline void vg_deregister(const vg_member&) { }
#endif

/* Interface dependent on Boost version:
 *
 * === namespace aliases ===
 *
 * ctxn:
 *     namespace of context library; alias for boost::context or boost::ctx
 *
 * === types ===
 *
 * context:
 *     execution context; either fcontext_t or fcontext_t*
 *
 * converted_context:
 *     additional member for converted_cs_thread;
 *     needed if context is defined as fcontext_t*
 *
 * ctx_stack_info:
 *     result of new_stack(), needed to delete stack properly in some versions
 *
 * stack_allocator:
 *     a stack allocator for cs_thread instances
 *
 * === functions ===
 *
 * void init_converted_context(converted_context&, context&)
 *
 * void ctx_switch(context&, context&, cst_impl*):
 *     implements the context switching from one cs_thread to another
 *
 * ctx_stack_info new_stack(context&, stack_allocator&, vg_member&):
 *     allocates a stack, prepares execution of context
 *     and (optionally) registers the new stack to valgrind
 *
 * void del_stack(stack_allocator&, ctx_stack_info, vg_member&):
 *     destroys the stack and (optionally) deregisters it from valgrind
 */
#if BOOST_VERSION >= 105400
    // === namespace aliases ===
    namespace ctxn = boost::context;
    // === types ===
    typedef ctxn::fcontext_t*                  context;
    typedef ctxn::fcontext_t                   converted_context;
    typedef boost::coroutines::stack_context   ctx_stack_info;
    typedef boost::coroutines::stack_allocator stack_allocator;
    // === functions ===
    inline void init_converted_context(converted_context& cctx, context& ctx) {
        ctx = &cctx;
    }
    inline void ctx_switch(context& from, context& to, cst_impl* ptr) {
        ctxn::jump_fcontext(from, to, (intptr_t) ptr);
    }
    ctx_stack_info new_stack(context& ctx,
                                     stack_allocator& alloc,
                                     vg_member& vgm) {
        auto mss = static_cast<intptr_t>(  stack_allocator::minimum_stacksize()
                                         * stack_multiplier);
        ctx_stack_info sinf;
        alloc.allocate(sinf, static_cast<size_t>(mss));
        ctx = ctxn::make_fcontext(sinf.sp, sinf.size, cst_trampoline);
        vg_register(vgm,
                    ctx->fc_stack.sp,
                    reinterpret_cast<vptr>(
                        reinterpret_cast<intptr_t>(ctx->fc_stack.sp) - mss));
        return sinf;
    }
    inline void del_stack(stack_allocator& alloc,
                          ctx_stack_info sctx,
                          vg_member& vgm) {
        vg_deregister(vgm);
        alloc.deallocate(sctx);
    }
#else
#   error libcppa context switching requires Boost in version >= 1.54
#endif

} // namespace <anonymous>

// base class for cs_thread pimpls
struct cst_impl {

    virtual ~cst_impl();

    virtual void run() = 0;

    inline void swap(cst_impl* to) {
        ctx_switch(m_ctx, to->m_ctx, to);
    }

    context m_ctx;

};

cst_impl::~cst_impl() { }

namespace {

// a cs_thread representing a thread ('converts' the thread to a cs_thread)
struct converted_cs_thread : cst_impl {

    converted_cs_thread() {
        init_converted_context(m_converted, m_ctx);
    }

    void run() override {
        throw std::logic_error("converted_cs_thread::run called");
    }

    converted_context m_converted;

};

// a cs_thread executing a function
struct fun_cs_thread : cst_impl {

    fun_cs_thread(cst_fun fun, vptr arg) : m_fun(fun), m_arg(arg) {
        m_stack_info = new_stack(m_ctx, m_alloc, m_vgm);
    }

    ~fun_cs_thread() {
        del_stack(m_alloc, m_stack_info, m_vgm);
    }

    void run() override {
        m_fun(m_arg);
    }

    cst_fun         m_fun;        // thread function
    vptr            m_arg;        // argument for thread function invocation
    stack_allocator m_alloc;      // allocates memory for our stack
    vg_member       m_vgm;        // valgrind meta informations (optionally)
    ctx_stack_info  m_stack_info; // needed to delete stack in destructor

};

} // namespace <anonymous>

void cst_trampoline(intptr_t iptr) {
    auto ptr = (cst_impl*) iptr;
    ptr->run();
}

cs_thread::cs_thread() : m_impl(new converted_cs_thread) { }

cs_thread::cs_thread(cst_fun f, vptr x) : m_impl(new fun_cs_thread(f, x)) { }

void cs_thread::swap(cs_thread& from, cs_thread& to) {
    from.m_impl->swap(to.m_impl);
}

cs_thread::~cs_thread() {
    delete m_impl;
}

const bool cs_thread::is_disabled_feature = false;

} } // namespace cppa::detail

#endif // CPPA_DISABLE_CONTEXT_SWITCHING
