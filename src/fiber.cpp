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


#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include "cppa/config.hpp"
#ifndef CPPA_DISABLE_CONTEXT_SWITCHING

#include <atomic>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include "cppa/util/fiber.hpp"
#include "cppa/detail/thread.hpp"

#ifdef CPPA_USE_UCONTEXT_IMPL

#include <signal.h>
#include <sys/mman.h>

#include <ucontext.h>

namespace {

constexpr size_t s_stack_size = SIGSTKSZ;

inline void* get_stack()
{
    return mmap(0,
                s_stack_size,
                PROT_EXEC | PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON,
                -1,
                0);
}

inline void release_stack(void* mem)
{
     munmap(mem, s_stack_size);
}

typedef void (*void_function)();
typedef void (*void_ptr_function)(void*);

template<size_t PointerSize,
         typename FunType = void_function,
         typename IntType = int>
struct trampoline;

template<typename FunType, typename IntType>
struct trampoline<4, FunType, IntType>
{
    static_assert(sizeof(int) == 4, "sizeof(int) != 4");
    static void bounce(int func_ptr, int ptr)
    {
        auto func = reinterpret_cast<void_ptr_function>(func_ptr);
        func(reinterpret_cast<void*>(ptr));
    }
    static void prepare(ucontext_t* ctx, void_ptr_function func, void* arg)
    {
        makecontext(ctx, (FunType) &trampoline<4>::bounce,
                    2, (IntType) func, (IntType) arg);
    }
};

template<typename FunType, typename IntType>
struct trampoline<8, FunType, IntType>
{
    static_assert(sizeof(int) == 4, "sizeof(int) != 4");
    static void bounce(int func_first_4_byte, int func_second_4_byte,
                       int arg_first_4_byte, int arg_second_4_byte)
    {
        std::uint64_t arg_ptr;
        {
            int* _addr = reinterpret_cast<int*>(&arg_ptr);
            _addr[0] = arg_first_4_byte;
            _addr[1] = arg_second_4_byte;
        }
        std::uint64_t func_ptr;
        {
            int* _addr = reinterpret_cast<int*>(&func_ptr);
            _addr[0] = func_first_4_byte;
            _addr[1] = func_second_4_byte;
        }
        auto func = reinterpret_cast<void_ptr_function>(func_ptr);
        func((void*) arg_ptr);
    }
    static void prepare(ucontext_t* ctx, void_ptr_function func, void* arg)
    {
        std::uint64_t arg_ptr = reinterpret_cast<std::uint64_t>(arg);
        std::uint64_t func_ptr = reinterpret_cast<std::uint64_t>(func);
        int* func_addr = reinterpret_cast<int*>(&func_ptr);
        int* arg_addr = reinterpret_cast<int*>(&arg_ptr);
        makecontext(ctx, (FunType) &trampoline<8>::bounce,
                    4, func_addr[0], func_addr[1], arg_addr[0], arg_addr[1]);
    }
};

} // namespace <anonymous>

namespace cppa { namespace util {

struct fiber_impl
{

    bool m_initialized;
    void* m_stack;
    void (*m_func)(void*);
    void* m_arg;
    ucontext_t m_ctx;

    fiber_impl() throw() : m_initialized(true), m_stack(0), m_func(0), m_arg(0)
    {
        memset(&m_ctx, 0, sizeof(ucontext_t));
        getcontext(&m_ctx);
    }

    fiber_impl(void (*func)(void*), void* arg)
        : m_initialized(false)
        , m_stack(nullptr)
        , m_func(func)
        , m_arg(arg)
    {
    }

    inline void swap(fiber_impl* to)
    {
        swapcontext(&(m_ctx), &(to->m_ctx));
    }

    void initialize()
    {
        m_initialized = true;
        memset(&m_ctx, 0, sizeof(ucontext_t));
        getcontext(&m_ctx);
        m_stack = get_stack();
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = s_stack_size;
        m_ctx.uc_link = nullptr;
        trampoline<sizeof(void*)>::prepare(&m_ctx, m_func, m_arg);
    }

    inline void lazy_init()
    {
        if (!m_initialized) initialize();
    }

    ~fiber_impl()
    {
        if (m_stack) release_stack(m_stack);
    }

};

fiber::fiber() throw() : m_impl(new fiber_impl)
{
}

fiber::fiber(void (*func)(void*), void* arg) : m_impl(new fiber_impl(func, arg))
{
}

fiber::~fiber()
{
    delete m_impl;
}

void fiber::swap(fiber& from, fiber& to)
{
    to.m_impl->lazy_init();
    from.m_impl->swap(to.m_impl);
}

} } // namespace cppa::util

#elif defined(CPPA_USE_FIBER_IMPL)

#include <stdexcept>

fiber::fiber() : handle(nullptr), is_converted_thread(true)
{
    handle = ConvertThreadToFiber(nullptr);
    if (handle == nullptr)
    {
        throw std::logic_error("handle == nullptr");
    }
}

fiber::fiber(LPFIBER_START_ROUTINE func, LPVOID arg1)
    : handle(nullptr)
    , is_converted_thread(false)
{
    handle = CreateFiber(0, func, arg1);
    if (handle == nullptr)
    {
        throw std::logic_error("handle == nullptr");
    }
}

fiber::~fiber()
{
    if (handle != nullptr && !is_converted_thread)
    {
        DeleteFiber(handle);
    }
}

#endif // CPPA_USE_FIBER_IMPL

#else // ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace { int keep_compiler_happy() { return 42; } }

#endif // CPPA_DISABLE_CONTEXT_SWITCHING
