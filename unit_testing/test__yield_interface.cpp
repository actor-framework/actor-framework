#include <atomic>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include "test.hpp"
#include "cppa/util/fiber.hpp"
#include "cppa/detail/yield_interface.hpp"

#include <boost/context/all.hpp>


namespace cppa { namespace detail {

std::ostream& operator<<(std::ostream& o, yield_state ys) {
    switch (ys) {
        case yield_state::invalid:
            return (o << "yield_state::invalid");
        case yield_state::ready:
            return (o << "yield_state::ready");
        case yield_state::blocked:
            return (o << "yield_state::blocked");
        case yield_state::done:
            return (o << "yield_state::done");
        default:
            return (o << "{{{invalid yield_state}}}");
    }
}

} } // namespace cppa::detail

using namespace cppa;
using namespace cppa::util;
using namespace cppa::detail;

struct pseudo_worker {

    int m_count;
    bool m_blocked;

    pseudo_worker() : m_count(0), m_blocked(true) { }

    void operator()() {
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

void coroutine(void* worker) { (*reinterpret_cast<pseudo_worker*>(worker))();
}

size_t test__yield_interface() {
    CPPA_TEST(test__yield_interface);
#   ifdef CPPA_DISABLE_CONTEXT_SWITCHING
    cout << "WARNING: context switching was explicitly disabled using "
            "CPPA_DISABLE_CONTEXT_SWITCHING"
         << endl;
#   else
    fiber fself;
    pseudo_worker worker;
    fiber fcoroutine(coroutine, &worker);
    yield_state ys;
    int i = 0;
    do {
        if (i == 2) worker.m_blocked = false;
        ys = call(&fcoroutine, &fself);
        ++i;
    }
    while (ys != yield_state::done && i < 12);
    CPPA_CHECK_EQUAL(yield_state::done, ys);
    CPPA_CHECK_EQUAL(10, worker.m_count);
    CPPA_CHECK_EQUAL(12, i);
#   endif
    return CPPA_TEST_RESULT;
}
