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


#ifndef CPPA_CONFIG_HPP
#define CPPA_CONFIG_HPP

// uncomment this line or use define CPPA_DISABLE_CONTEXT_SWITCHING using CMAKE
// if boost.context is not available on your platform
//#define CPPA_DISABLE_CONTEXT_SWITCHING

#if defined(__GNUC__)
#  define CPPA_GCC
#endif

#if defined(__APPLE__)
#  define CPPA_MACOS
#  ifndef _GLIBCXX_HAS_GTHREADS
#    define _GLIBCXX_HAS_GTHREADS
#  endif
#elif defined(__GNUC__) && defined(__linux__)
#  define CPPA_LINUX
#elif defined(WIN32)
#  define CPPA_WINDOWS
#else
#  error Plattform and/or compiler not supportet
#endif

#include <cstdio>
#include <cstdlib>

#ifdef CPPA_DEBUG
#include <execinfo.h>

#define CPPA_REQUIRE__(stmt, file, line)                                       \
    printf("%s:%u: requirement failed '%s'\n", file, line, stmt);              \
    {                                                                          \
        void *array[10];                                                       \
        size_t size = backtrace(array, 10);                                    \
        backtrace_symbols_fd(array, size, 2);                                  \
    }                                                                          \
    abort()
#define CPPA_REQUIRE(stmt)                                                     \
    if ((stmt) == false) {                                                     \
        CPPA_REQUIRE__(#stmt, __FILE__, __LINE__);                             \
    }((void) 0)
#else // CPPA_DEBUG
#define CPPA_REQUIRE(unused) ((void) 0)
#endif // CPPA_DEBUG

#define CPPA_CRITICAL__(error, file, line) {                                   \
        printf("%s:%u: critical error: '%s'\n", file, line, error);            \
        exit(7);                                                               \
    } ((void) 0)

#define CPPA_CRITICAL(error) CPPA_CRITICAL__(error, __FILE__, __LINE__)

#endif // CPPA_CONFIG_HPP
