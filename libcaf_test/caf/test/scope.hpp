// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/test/fwd.hpp"

#include "caf/detail/test_export.hpp"

namespace caf::test {

/// Represents an execution scope for a test block.
class CAF_TEST_EXPORT scope {
public:
  scope() noexcept = default;

  explicit scope(block* ptr) noexcept : ptr_(ptr) {
    // nop
  }

  scope(scope&& other) noexcept : ptr_(other.release()) {
    // nop
  }

  scope& operator=(scope&& other) noexcept;

  scope(const scope&) = delete;

  scope& operator=(const scope&) = delete;

  ~scope();

  /// Leave the scope with calling `on_leave` before `leave`. This allows the
  /// block to perform sanity checks and potentially throws.
  void leave();

  /// Checks whether this scope is active.
  explicit operator bool() const noexcept {
    return ptr_ != nullptr;
  }

private:
  block* release() noexcept {
    auto tmp = ptr_;
    ptr_ = nullptr;
    return tmp;
  }

  block* ptr_ = nullptr;
};

} // namespace caf::test
