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
  /// Creates a tracking resource that forwards allocations to `upstream`.
  explicit tracking_resource(std::pmr::memory_resource* upstream
                             = std::pmr::new_delete_resource());

  /// Returns the number of allocations made through this resource.
  [[nodiscard]] std::size_t allocations() const noexcept;

  /// Returns the number of deallocations made through this resource.
  [[nodiscard]] std::size_t deallocations() const noexcept;

  /// Returns the number of allocations not yet deallocated.
  [[nodiscard]] std::size_t outstanding_allocations() const noexcept;

  /// Returns the number of bytes allocated but not yet deallocated.
  [[nodiscard]] std::size_t outstanding_bytes() const noexcept;

private:
  void* do_allocate(std::size_t bytes, std::size_t alignment) override;

  void do_deallocate(void* ptr, std::size_t bytes,
                     std::size_t alignment) override;

  bool
  do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

  std::pmr::memory_resource* upstream_;
  std::size_t allocations_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t deallocated_bytes_ = 0;
};

} // namespace caf::test
