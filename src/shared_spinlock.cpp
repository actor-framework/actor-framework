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


#include "cppa/config.hpp"

#include <limits>
#include <thread>
#include "cppa/util/shared_spinlock.hpp"

namespace {

constexpr long min_long = std::numeric_limits<long>::min();

} // namespace <anonymous>

namespace cppa { namespace util {

shared_spinlock::shared_spinlock() : m_flag(0) {

}

void shared_spinlock::lock() {
    long v = m_flag.load();
    for (;;) {
        if (v != 0) {
            //std::this_thread::yield();
            v = m_flag.load();
        }
        else if (m_flag.compare_exchange_weak(v, min_long)) {
            return;
        }
        // else: next iteration
    }
}

void shared_spinlock::lock_upgrade() {
    unlock_shared();
    lock();
}

void shared_spinlock::unlock() {
    m_flag.store(0);
}

bool shared_spinlock::try_lock() {
    long v = m_flag.load();
    return (v == 0) ? m_flag.compare_exchange_weak(v, min_long) : false;
}

void shared_spinlock::lock_shared() {
    long v = m_flag.load();
    for (;;) {
        if (v < 0) {
            //std::this_thread::yield();
            v = m_flag.load();
        }
        else if (m_flag.compare_exchange_weak(v, v + 1)) {
            return;
        }
        // else: next iteration
    }
}

void shared_spinlock::unlock_shared() {
    m_flag.fetch_sub(1);
}

bool shared_spinlock::try_lock_shared() {
    long v = m_flag.load();
    return (v >= 0) ? m_flag.compare_exchange_weak(v, v + 1) : false;
}

} } // namespace cppa::util
