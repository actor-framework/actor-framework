#include <atomic>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include "test.hpp"
#include "cppa/detail/cs_thread.hpp"
#include "cppa/detail/yield_interface.hpp"

using namespace cppa;
using namespace cppa::util;
using namespace cppa::detail;

struct pseudo_worker {

    int m_count;
    bool m_blocked;

    pseudo_worker() : m_count(0), m_blocked(true) { }

    void run () {
        for (;;) {
            if (m_blocked) {
                yield(yield_state::blocked);
            }
            else {
                ++m_count;
                yield(m_count < 10 ? yield_state::ready : yield_state::done);
            }
        }
    }

};

void coroutine (void* worker) {
    reinterpret_cast<pseudo_worker*>(worker)->run();
}

int main() {
    CPPA_TEST(test_yield_interface);
#   ifndef CPPA_ENABLE_CONTEXT_SWITCHING
    CPPA_PRINT("WARNING: context switching disabled by default, "
               "enable by defining CPPA_ENABLE_CONTEXT_SWITCHING");
#   else
    cs_thread fself;
    pseudo_worker worker;
    cs_thread fcoroutine(coroutine, &worker);
    yield_state ys;
    int i = 0;
    do {
        if (i == 2) worker.m_blocked = false;
        ys = call(&fcoroutine, &fself);
        ++i;
    }
    while (ys != yield_state::done && i < 12);
    CPPA_CHECK_EQUAL(to_string(ys), "yield_state::done");
    CPPA_CHECK_EQUAL(worker.m_count, 10);
    CPPA_CHECK_EQUAL(i, 12);
#   endif
    return CPPA_TEST_RESULT();
}
