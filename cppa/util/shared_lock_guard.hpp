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


#ifndef CPPA_SHARED_LOCK_GUARD_HPP
#define CPPA_SHARED_LOCK_GUARD_HPP

namespace cppa { namespace util {

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

} } // namespace cppa::util

#endif // CPPA_SHARED_LOCK_GUARD_HPP
