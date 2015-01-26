/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_LOCKS_HPP
#define CAF_DETAIL_LOCKS_HPP

#include <mutex>

namespace caf {

template <class Lockable>
using unique_lock = std::unique_lock<Lockable>;

template <class SharedLockable>
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

template <class SharedLockable>
using upgrade_lock = shared_lock<SharedLockable>;

template <class UpgradeLockable>
class upgrade_to_unique_lock {

 public:

  using lockable = UpgradeLockable;

  template <class LockType>
  upgrade_to_unique_lock(LockType& other) {
    m_lockable = other.release();
    if (m_lockable) m_lockable->unlock_upgrade_and_lock();
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
