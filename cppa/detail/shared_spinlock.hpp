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

#ifndef CPPA_SHARED_SPINLOCK_HPP
#define CPPA_SHARED_SPINLOCK_HPP

#include <atomic>
#include <cstddef>

namespace cppa {
namespace detail {

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

    void lock_upgrade();
    void unlock_upgrade();
    void unlock_upgrade_and_lock();
    void unlock_and_lock_upgrade();

};

} // namespace detail
} // namespace cppa

#endif // CPPA_SHARED_SPINLOCK_HPP
