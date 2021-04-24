// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "caf/detail/core_export.hpp"

namespace caf::detail {

// Drop-in replacement for C++20's std::latch.
class CAF_CORE_EXPORT latch {
public:
  explicit latch(ptrdiff_t value) : count_(value) {
    // nop
  }

  latch(const latch&) = delete;

  latch& operator=(const latch&) = delete;

  void count_down_and_wait();

  void wait();

  void count_down();

  bool is_ready() const noexcept;

private:
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  ptrdiff_t count_;
};

} // namespace caf::detail
