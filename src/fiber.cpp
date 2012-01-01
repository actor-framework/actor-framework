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

#include <atomic>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include <ucontext.h>
#include <sys/mman.h>
#include <signal.h>
#include <cstddef>

#include "cppa/util/fiber.hpp"

#ifdef CPPA_USE_UCONTEXT_IMPL

namespace {

constexpr size_t STACK_SIZE = SIGSTKSZ;

struct stack_node
{
    stack_node* next;
    void* s;

    stack_node(): next(NULL) , s(mmap(0, STACK_SIZE,
                                         PROT_EXEC | PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANON,
                                         -1,
                                         0))
                                 //new char[STACK_SIZE])
    {
        //memset(s, 0, STACK_SIZE);
    }

    ~stack_node()
    {
        munmap(s, STACK_SIZE);
        //delete[] reinterpret_cast<char*>(s);
    }
};

std::atomic<stack_node*> s_head(nullptr);
std::atomic<size_t> s_size(0);

struct cleanup_helper
{
    ~cleanup_helper()
    {
        stack_node* h = s_head.load();
        while (s_head.compare_exchange_weak(h, nullptr) == false)
        {
            // try again
        }
        s_size = 0;
        while (h)
        {
            auto tmp = h->next;
            delete h;
            h = tmp;
        }
    }
}
s_cleanup_helper;

stack_node* get_stack_node()
{
    stack_node* result = s_head.load();
    for (;;)
    {
        if (result)
        {
            if (s_head.compare_exchange_weak(result, result->next))
            {
                --s_size;
                return result;
            }
        }
        else
        {
            return new stack_node;
        }
    }
}

void release_stack_node(stack_node* node)
{
    if (++s_size < 16)
    {
        node->next = s_head.load();
        for (;;)
        {
            if (s_head.compare_exchange_weak(node->next, node)) return;
        }
    }
    else
    {
        --s_size;
        delete node;
    }
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
    stack_node* m_node;
    void (*m_func)(void*);
    void* m_arg;
    ucontext_t m_ctx;

    fiber_impl() throw() : m_initialized(true), m_node(0), m_func(0), m_arg(0)
    {
        memset(&m_ctx, 0, sizeof(ucontext_t));
        getcontext(&m_ctx);
    }

    fiber_impl(void (*func)(void*), void* arg)
        : m_initialized(false)
        , m_node(nullptr)
        , m_func(func)
        , m_arg(arg)
    {
    }

    void initialize()
    {
        m_initialized = true;
        m_node = get_stack_node();
        memset(&m_ctx, 0, sizeof(ucontext_t));
        getcontext(&m_ctx);
        m_ctx.uc_stack.ss_sp = m_node->s;
        m_ctx.uc_stack.ss_size = STACK_SIZE;
        m_ctx.uc_link = nullptr;
        trampoline<sizeof(void*)>::prepare(&m_ctx, m_func, m_arg);
    }

    inline void lazy_init()
    {
        if (!m_initialized) initialize();
    }

    ~fiber_impl()
    {
        if (m_node) release_stack_node(m_node);
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
    swapcontext(&(from.m_impl->m_ctx), &(to.m_impl->m_ctx));
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

#endif
