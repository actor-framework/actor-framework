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


#ifndef CPPA_UTIL_UPGRADE_LOCK_GUARD_HPP
#define CPPA_UTIL_UPGRADE_LOCK_GUARD_HPP

namespace cppa {
namespace util {

/**
 * @brief Upgrades shared ownership to exclusive ownership.
 */
template<typename UpgradeLockable>
class upgrade_lock_guard {

    UpgradeLockable* m_lockable;

 public:

    template<template<typename> class LockType>
    upgrade_lock_guard(LockType<UpgradeLockable>& other) {
        m_lockable = other.release();
        if (m_lockable) m_lockable->lock_upgrade();
    }

    ~upgrade_lock_guard() {
        if (m_lockable) m_lockable->unlock();
    }

    bool owns_lock() const {
        return m_lockable != nullptr;
    }

};

} // namespace util
} // namespace cppa


#endif // CPPA_UTIL_UPGRADE_LOCK_GUARD_HPP
