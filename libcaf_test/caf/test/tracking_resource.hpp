// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/test_export.hpp"

#include <cstddef>
#include <memory_resource>

namespace caf::test {

/// A memory resource that tracks all allocations and deallocations.
class CAF_TEST_EXPORT tracking_resource : public std::pmr::memory_resource {
public:
  /// Creates a tracking resource that forwards allocations to
  /// `std::pmr::new_delete_resource()`.
  tracking_resource() noexcept = default;

  /// Creates a tracking resource that forwards allocations to `upstream`.
  explicit tracking_resource(std::pmr::memory_resource& upstream) noexcept
    : upstream_(&upstream) {
    // nop
  }

  /// Returns the number of allocations made through this resource.
  [[nodiscard]] size_t allocations() const noexcept {
    return allocations_;
  }

  /// Returns the number of deallocations made through this resource.
  [[nodiscard]] size_t deallocations() const noexcept {
    return deallocations_;
  }

  /// Returns the number of allocations not yet deallocated.
  [[nodiscard]] size_t outstanding_allocations() const noexcept {
    return allocations_ - deallocations_;
  }

  /// Returns the number of bytes allocated but not yet deallocated.
  [[nodiscard]] size_t outstanding_bytes() const noexcept {
    return allocated_bytes_ - deallocated_bytes_;
  }

private:
  void* do_allocate(size_t bytes, size_t alignment) override;

  void do_deallocate(void* ptr, size_t bytes, size_t alignment) override;

  bool
  do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

  std::pmr::memory_resource* upstream_ = std::pmr::new_delete_resource();
  size_t allocations_ = 0;
  size_t deallocations_ = 0;
  size_t allocated_bytes_ = 0;
  size_t deallocated_bytes_ = 0;
};

} // namespace caf::test
