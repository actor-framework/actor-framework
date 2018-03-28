/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <atomic>
#include <cstddef>

namespace caf {
namespace detail {

/// A spinlock implementation providing shared and exclusive locking.
class shared_spinlock {

  std::atomic<long> flag_;

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
} // namespace caf

