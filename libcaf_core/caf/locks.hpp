// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <mutex>

namespace caf {

template <class Lockable>
using unique_lock = std::unique_lock<Lockable>;

template <class SharedLockable>
class shared_lock {
public:
  using lockable = SharedLockable;

  explicit shared_lock(lockable& arg) : lockable_(&arg) {
    lockable_->lock_shared();
  }

  ~shared_lock() {
    unlock();
  }

  bool owns_lock() const {
    return lockable_ != nullptr;
  }

  void unlock() {
    if (lockable_) {
      lockable_->unlock_shared();
      lockable_ = nullptr;
    }
  }

  lockable* release() {
    auto result = lockable_;
    lockable_ = nullptr;
    return result;
  }

private:
  lockable* lockable_;
};

template <class SharedLockable>
using upgrade_lock = shared_lock<SharedLockable>;

template <class UpgradeLockable>
class upgrade_to_unique_lock {
public:
  using lockable = UpgradeLockable;

  template <class LockType>
  explicit upgrade_to_unique_lock(LockType& other) {
    lockable_ = other.release();
    if (lockable_)
      lockable_->unlock_upgrade_and_lock();
  }

  ~upgrade_to_unique_lock() {
    unlock();
  }

  bool owns_lock() const {
    return lockable_ != nullptr;
  }

  void unlock() {
    if (lockable_) {
      lockable_->unlock();
      lockable_ = nullptr;
    }
  }

private:
  lockable* lockable_;
};

} // namespace caf
