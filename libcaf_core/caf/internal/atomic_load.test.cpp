// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/internal/atomic_load.hpp"

#include "caf/test/test.hpp"

#include <cstdint>
#include <limits>

using namespace caf;

TEST("atomic loads preserve integer and pointer values") {
  uint8_t a = UINT8_MAX;
  uint16_t b = UINT16_MAX;
  uint32_t c = UINT32_MAX;
  uint64_t d = UINT64_MAX;
  int64_t e = std::numeric_limits<int64_t>::min();
  const int fixed = 42;
  int value = 42;
  int* ptr = nullptr;
  check_eq(internal::atomic_load_relaxed(a), a);
  check_eq(internal::atomic_load_relaxed(b), b);
  check_eq(internal::atomic_load_relaxed(c), c);
  check_eq(internal::atomic_load_relaxed(d), d);
  check_eq(internal::atomic_load_relaxed(e), e);
  check_eq(internal::atomic_load_relaxed(fixed), fixed);
  check_eq(internal::atomic_load_relaxed(ptr), nullptr);
  ptr = &value;
  check_eq(internal::atomic_load_relaxed(ptr), &value);
}
