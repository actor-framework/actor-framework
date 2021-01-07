// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config.hpp"

#include <limits>
#include <thread>

#include "caf/detail/shared_spinlock.hpp"

#include "caf/detail/cas_weak.hpp"

namespace {

inline long min_long() {
  return std::numeric_limits<long>::min();
}

} // namespace

namespace caf::detail {

shared_spinlock::shared_spinlock() : flag_(0) {
  // nop
}

void shared_spinlock::lock() {
  long v = flag_.load();
  for (;;) {
    if (v != 0) {
      v = flag_.load();
    } else if (cas_weak(&flag_, &v, min_long())) {
      return;
    }
    // else: next iteration
  }
}

void shared_spinlock::lock_upgrade() {
  lock_shared();
}

void shared_spinlock::unlock_upgrade() {
  unlock_shared();
}

void shared_spinlock::unlock_upgrade_and_lock() {
  unlock_shared();
  lock();
}

void shared_spinlock::unlock_and_lock_upgrade() {
  unlock();
  lock_upgrade();
}

void shared_spinlock::unlock() {
  flag_.store(0);
}

bool shared_spinlock::try_lock() {
  long v = flag_.load();
  return (v == 0) ? cas_weak(&flag_, &v, min_long()) : false;
}

void shared_spinlock::lock_shared() {
  long v = flag_.load();
  for (;;) {
    if (v < 0) {
      // std::this_thread::yield();
      v = flag_.load();
    } else if (cas_weak(&flag_, &v, v + 1)) {
      return;
    }
    // else: next iteration
  }
}

void shared_spinlock::unlock_shared() {
  flag_.fetch_sub(1);
}

bool shared_spinlock::try_lock_shared() {
  long v = flag_.load();
  return (v >= 0) ? cas_weak(&flag_, &v, v + 1) : false;
}

} // namespace caf::detail
