// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/atomic.hpp"

#include "caf/test/test.hpp"

#include <cstdint>
#include <limits>
#include <thread>

using namespace caf;

template <class T>
using limits = std::numeric_limits<T>;

TEST("atomic loads preserve integer and pointer values") {
  uint8_t a = limits<uint8_t>::max();
  uint16_t b = limits<uint16_t>::max();
  uint32_t c = limits<uint32_t>::max();
  uint64_t d = limits<uint64_t>::max();
  int64_t e = limits<int64_t>::min();
  const int fixed = 42;
  int value = 42;
  int* ptr = nullptr;
  check_eq(detail::atomic_load_acquire(a), a);
  check_eq(detail::atomic_load_acquire(b), b);
  check_eq(detail::atomic_load_acquire(c), c);
  check_eq(detail::atomic_load_acquire(d), d);
  check_eq(detail::atomic_load_acquire(e), e);
  check_eq(detail::atomic_load_acquire(fixed), fixed);
  check_eq(detail::atomic_load_acquire(ptr), nullptr);
  detail::atomic_store_release(ptr, &value);
  check_eq(detail::atomic_load_acquire(ptr), &value);
}

TEST("atomic stores preserve integer and pointer values") {
  uint8_t a = 0;
  uint16_t b = 0;
  uint32_t c = 0;
  uint64_t d = 0;
  int64_t e = 0;
  int value = 42;
  int* ptr = nullptr;
  detail::atomic_store_release(a, limits<uint8_t>::max());
  detail::atomic_store_release(b, limits<uint16_t>::max());
  detail::atomic_store_release(c, limits<uint32_t>::max());
  detail::atomic_store_release(d, limits<uint64_t>::max());
  detail::atomic_store_release(e, limits<int64_t>::min());
  detail::atomic_store_release(ptr, &value);
  check_eq(detail::atomic_load_acquire(a), limits<uint8_t>::max());
  check_eq(detail::atomic_load_acquire(b), limits<uint16_t>::max());
  check_eq(detail::atomic_load_acquire(c), limits<uint32_t>::max());
  check_eq(detail::atomic_load_acquire(d), limits<uint64_t>::max());
  check_eq(detail::atomic_load_acquire(e), limits<int64_t>::min());
  check_eq(detail::atomic_load_acquire(ptr), &value);
}
