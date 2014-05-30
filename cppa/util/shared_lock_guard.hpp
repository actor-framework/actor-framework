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


#ifndef CPPA_UTIL_SHARED_LOCK_GUARD_HPP
#define CPPA_UTIL_SHARED_LOCK_GUARD_HPP

namespace cppa {
namespace util {

/**
 * @brief Similar to <tt>std::lock_guard</tt> but performs shared locking.
 */
template<typename SharedLockable>
class shared_lock_guard {

    SharedLockable* m_lockable;

 public:

    explicit shared_lock_guard(SharedLockable& lockable) : m_lockable(&lockable) {
        m_lockable->lock_shared();
    }

    ~shared_lock_guard() {
        if (m_lockable) m_lockable->unlock_shared();
    }

    bool owns_lock() const {
        return m_lockable != nullptr;
    }

    SharedLockable* release() {
        auto result = m_lockable;
        m_lockable = nullptr;
        return result;
    }
};

} // namespace util
} // namespace cppa


#endif // CPPA_UTIL_SHARED_LOCK_GUARD_HPP
