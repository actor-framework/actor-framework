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

#ifndef CAF_DETAIL_LOCKS_HPP
#define CAF_DETAIL_LOCKS_HPP

#include <mutex>

namespace caf {

template<class Lockable>
using unique_lock = std::unique_lock<Lockable>;

template<class SharedLockable>
class shared_lock {

 public:

    using lockable = SharedLockable;

    explicit shared_lock(lockable& arg) : m_lockable(&arg) {
        m_lockable->lock_shared();
    }

    ~shared_lock() {
        if (m_lockable) m_lockable->unlock_shared();
    }

    bool owns_lock() const {
        return m_lockable != nullptr;
    }

    lockable* release() {
        auto result = m_lockable;
        m_lockable = nullptr;
        return result;
    }

 private:

    lockable* m_lockable;

};

template<class SharedLockable>
using upgrade_lock = shared_lock<SharedLockable>;

template<class UpgradeLockable>
class upgrade_to_unique_lock {

 public:

    using lockable = UpgradeLockable;

    template<template<typename> class LockType>
    upgrade_to_unique_lock(LockType<lockable>& other) {
        m_lockable = other.release();
        if (m_lockable) m_lockable->lock_upgrade();
    }

    ~upgrade_to_unique_lock() {
        if (m_lockable) m_lockable->unlock();
    }

    bool owns_lock() const {
        return m_lockable != nullptr;
    }

 private:

    lockable* m_lockable;

};

} // namespace caf

#endif // CAF_DETAIL_LOCKS_HPP
