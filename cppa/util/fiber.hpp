#ifndef FIBER_HPP
#define FIBER_HPP

#include "cppa/config.hpp"

#if defined (CPPA_MACOS) || defined (CPPA_LINUX)
#   define CPPA_USE_UCONTEXT_IMPL
#elif defined (CPPA_WINDOWS)
#   define CPPA_USE_FIBER_IMPL
#endif

#ifdef CPPA_USE_UCONTEXT_IMPL

namespace cppa { namespace util {

struct fiber_impl;

class fiber
{

    fiber_impl* m_impl;

public:

    fiber() throw();

    fiber(void (*func)(void*), void* arg1);

    ~fiber();

    static void swap(fiber& from, fiber& to);

};

} } // namespace cppa::util

#elif defined(CPPA_USE_FIBER_IMPL)

#include <Winsock2.h>
#include <Windows.h>

namespace cppa { namespace util {

class fiber
{

    LPVOID handle;

    // true if this fiber was created with ConvertThreadToFiber
    bool is_converted_thread;

    void init();

public:

    fiber();
    fiber(LPFIBER_START_ROUTINE func, LPVOID arg1);

    ~fiber();

    inline static void swap(fiber&, fiber& to)
    {
        SwitchToFiber(to.handle);
    }

};

} } // namespace cppa::util
#endif // CPPA_USE_UCONTEXT_IMPL



#endif // FIBER_HPP
