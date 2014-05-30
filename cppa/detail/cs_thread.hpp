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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_DETAIL_CS_THREAD_HPP
#define CPPA_DETAIL_CS_THREAD_HPP

namespace cppa {
namespace detail {

struct cst_impl;

// A cooperatively scheduled thread implementation
struct cs_thread {

    // Queries whether libcppa was compiled without cs threads on this platform.
    static const bool is_disabled_feature;

    // Creates a new cs_thread storing the context of the calling thread.
    cs_thread();

    // Creates a cs_thread that executes @p func(arg1)
    cs_thread(void (*func)(void*), void* arg1);

    ~cs_thread();

    // Swaps the context from @p source to @p target.
    static void swap(cs_thread& source, cs_thread& target);

    // pimpl
    cst_impl* m_impl;

};

} // namespace detail
} // namespace cppa

#endif // CPPA_DETAIL_CS_THREAD_HPP
