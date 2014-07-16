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

#include "caf/config.hpp"

#include <limits>
#include <thread>
#include "caf/detail/shared_spinlock.hpp"

namespace {

inline long min_long() { return std::numeric_limits<long>::min(); }

} // namespace <anonymous>

namespace caf {
namespace detail {

shared_spinlock::shared_spinlock() : m_flag(0) {}

void shared_spinlock::lock() {
    long v = m_flag.load();
    for (;;) {
        if (v != 0) {
            // std::this_thread::yield();
            v = m_flag.load();
        } else if (m_flag.compare_exchange_weak(v, min_long())) {
            return;
        }
        // else: next iteration
    }
}

void shared_spinlock::lock_upgrade() { lock_shared(); }

void shared_spinlock::unlock_upgrade() { unlock_shared(); }

void shared_spinlock::unlock_upgrade_and_lock() {
    unlock_shared();
    lock();
}

void shared_spinlock::unlock_and_lock_upgrade() {
    unlock();
    lock_upgrade();
}

void shared_spinlock::unlock() { m_flag.store(0); }

bool shared_spinlock::try_lock() {
    long v = m_flag.load();
    return (v == 0) ? m_flag.compare_exchange_weak(v, min_long()) : false;
}

void shared_spinlock::lock_shared() {
    long v = m_flag.load();
    for (;;) {
        if (v < 0) {
            // std::this_thread::yield();
            v = m_flag.load();
        } else if (m_flag.compare_exchange_weak(v, v + 1)) {
            return;
        }
        // else: next iteration
    }
}

void shared_spinlock::unlock_shared() { m_flag.fetch_sub(1); }

bool shared_spinlock::try_lock_shared() {
    long v = m_flag.load();
    return (v >= 0) ? m_flag.compare_exchange_weak(v, v + 1) : false;
}

} // namespace detail
} // namespace caf
