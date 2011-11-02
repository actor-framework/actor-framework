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

#include <iostream>

#include "test.hpp"
#include "cppa/util/fiber.hpp"
#include "cppa/detail/yield_interface.hpp"

using namespace cppa;
using namespace cppa::util;
using namespace cppa::detail;

namespace { ucontext_t ctx[2]; }

struct pseudo_worker
{

    int m_count;

    pseudo_worker() : m_count(0) { }

    void operator()()
    {
        for (;;)
        {
            ++m_count;
            swapcontext(&ctx[1], &ctx[0]);
        }
    }

};

__thread pseudo_worker* t_worker = nullptr;

void coroutine()
{
    (*t_worker)();
}

size_t test__yield_interface()
{
    CPPA_TEST(test__yield_interface);

    void* coroutine_stack = mmap(0,
                                 SIGSTKSZ,
                                 PROT_EXEC | PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON,
                                 -1,
                                 0);

    pseudo_worker worker;
    t_worker = &worker;

    memset(&ctx[0], 0, sizeof(ucontext_t));
    getcontext(&ctx[0]);

    memset(&ctx[1], 0, sizeof(ucontext_t));
    getcontext(&ctx[1]);
    ctx[1].uc_stack.ss_sp = coroutine_stack;
    ctx[1].uc_stack.ss_size = SIGSTKSZ;
    ctx[1].uc_link = &ctx[0];
    makecontext(&ctx[1], coroutine, 0);

    auto do_switch = []() { swapcontext(&ctx[0], &ctx[1]); };

    //fiber fself;
    //fiber fcoroutine(coroutine, nullptr);
    /*
    auto do_switch = [&]() { call(&fcoroutine, &fself); };
    do_switch();
    CPPA_CHECK(yielded_state() == yield_state::invalid);
    do_switch();
    CPPA_CHECK(yielded_state() == yield_state::ready);
    do_switch();
    CPPA_CHECK(yielded_state() == yield_state::blocked);
    do_switch();
    CPPA_CHECK(yielded_state() == yield_state::done);
    do_switch();
    CPPA_CHECK(yielded_state() == yield_state::killed);
    */

    //for (int i = 1 ; i < 11; ++i)
    while (worker.m_count < 11)
    {
        do_switch();
    }

    CPPA_CHECK_EQUAL(worker.m_count, 10);

    munmap(coroutine_stack, SIGSTKSZ);

    return CPPA_TEST_RESULT;
}
