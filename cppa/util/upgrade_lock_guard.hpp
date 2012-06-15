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


#ifndef CPPA_UPGRADE_LOCK_GUARD_HPP
#define CPPA_UPGRADE_LOCK_GUARD_HPP

namespace cppa { namespace util {

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

} } // namespace cppa::util

#endif // CPPA_UPGRADE_LOCK_GUARD_HPP
