// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <cstddef>

#include "caf/detail/core_export.hpp"

namespace caf::detail {

/// A spinlock implementation providing shared and exclusive locking.
class CAF_CORE_EXPORT shared_spinlock {
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

private:
  std::atomic<long> flag_;
};

} // namespace caf::detail
