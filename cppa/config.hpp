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


#ifndef CPPA_CONFIG_HPP
#define CPPA_CONFIG_HPP

// uncomment this line or use define CPPA_DISABLE_CONTEXT_SWITCHING using CMAKE
// if boost.context is not available on your platform
//#define CPPA_DISABLE_CONTEXT_SWITCHING

#if defined(__clang__)
#  define CPPA_CLANG
#  define CPPA_DEPRECATED __attribute__((__deprecated__))
#elif defined(__GNUC__)
#  define CPPA_GCC
#  define CPPA_DEPRECATED __attribute__((__deprecated__))
#elif defined(_MSC_VER)
#  define CPPA_DEPRECATED __declspec(deprecated)
#else
#  define CPPA_DEPRECATED
#endif

#if defined(__APPLE__)
#  define CPPA_MACOS
#  ifndef _GLIBCXX_HAS_GTHREADS
#    define _GLIBCXX_HAS_GTHREADS
#  endif
#elif defined(__linux__)
#  define CPPA_LINUX
#elif defined(WIN32)
#  define CPPA_WINDOWS
#else
#  error Plattform and/or compiler not supportet
#endif

#include <memory>
#include <cstdio>
#include <cstdlib>

#ifdef CPPA_DEBUG_MODE
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
#else // CPPA_DEBUG_MODE
#define CPPA_REQUIRE(unused) ((void) 0)
#endif // CPPA_DEBUG_MODE

#define CPPA_CRITICAL__(error, file, line) {                                   \
        printf("%s:%u: critical error: '%s'\n", file, line, error);            \
        exit(7);                                                               \
    } ((void) 0)

#define CPPA_CRITICAL(error) CPPA_CRITICAL__(error, __FILE__, __LINE__)

#ifdef CPPA_WINDOWS
#else
#   include <unistd.h>
#endif

namespace cppa {

/**
 * @brief An alternative for the 'missing' @p std::make_unqiue.
 */
template<typename T, typename... Args>
std::unique_ptr<T> create_unique(Args&&... args) {
    return std::unique_ptr<T>{new T(std::forward<Args>(args)...)};
}


#ifdef CPPA_WINDOWS
    typedef SOCKET native_socket_type;
    typedef const char* socket_send_ptr;
    typedef char* socket_recv_ptr;
    constexpr SOCKET invalid_socket = INVALID_SOCKET;
#else
    typedef int native_socket_type;
    typedef const void* socket_send_ptr;
    typedef void* socket_recv_ptr;
    constexpr int invalid_socket = -1;
    inline void closesocket(native_socket_type fd) { close(fd); }
#endif

} // namespace cppa

#endif // CPPA_CONFIG_HPP
