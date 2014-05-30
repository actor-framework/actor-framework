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


#ifndef CPPA_UTIL_SHARED_SPINLOCK_HPP
#define CPPA_UTIL_SHARED_SPINLOCK_HPP

#include <atomic>
#include <cstddef>

namespace cppa {
namespace util {

/**
 * @brief A spinlock implementation providing shared and exclusive locking.
 */
class shared_spinlock {

    std::atomic<long> m_flag;

 public:

    shared_spinlock();

    void lock();
    void unlock();
    bool try_lock();

    void lock_shared();
    void unlock_shared();
    bool try_lock_shared();

    /**
     * @brief Upgrades a shared lock to an exclusive lock (must be released
     *        through a call to {@link lock()} afterwards).
     */
    void lock_upgrade();

};

} // namespace util
} // namespace cppa


#endif // CPPA_UTIL_SHARED_SPINLOCK_HPP
