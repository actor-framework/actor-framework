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


#ifndef FIBER_HPP
#define FIBER_HPP

#include "cppa/config.hpp"

#if defined(CPPA_MACOS) || defined(CPPA_LINUX)
#   define CPPA_USE_UCONTEXT_IMPL
#elif defined (CPPA_WINDOWS)
#   define CPPA_USE_FIBER_IMPL
#endif

#ifdef CPPA_DISABLE_CONTEXT_SWITCHING

namespace cppa { namespace util {
class fiber {
 public:
    inline static void swap(fiber&, fiber&) { }
};
} } // namespace cppa::util

#elif defined(CPPA_USE_UCONTEXT_IMPL)

namespace cppa { namespace util {

struct fiber_impl;

class fiber {

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

class fiber {

    LPVOID handle;

    // true if this fiber was created with ConvertThreadToFiber
    bool is_converted_thread;

    void init();

public:

    fiber();
    fiber(LPFIBER_START_ROUTINE func, LPVOID arg1);

    ~fiber();

    inline static void swap(fiber&, fiber& to) {
        SwitchToFiber(to.handle);
    }

};

} } // namespace cppa::util
#endif // CPPA_USE_UCONTEXT_IMPL



#endif // FIBER_HPP
