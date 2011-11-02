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

struct pseudo_worker
{

    int m_count;

    pseudo_worker() : m_count(0) { }

    void operator()()
    {
        for (;;)
        {
            ++m_count;
            yield(m_count < 10 ? yield_state::ready : yield_state::done);
        }
    }

};

void coroutine(void* worker)
{
    (*reinterpret_cast<pseudo_worker*>(worker))();
}

size_t test__yield_interface()
{
    CPPA_TEST(test__yield_interface);

    pseudo_worker worker;

    fiber fself;
    fiber fcoroutine(coroutine, &worker);

    auto do_switch = [&]() { call(&fcoroutine, &fself); };

    int i = 0;
    do
    {
        do_switch();
        ++i;
    }
    while (yielded_state() != yield_state::done && i < 10);

    CPPA_CHECK_EQUAL(yielded_state(), yield_state::done);
    CPPA_CHECK_EQUAL(i, 10);

    return CPPA_TEST_RESULT;
}
