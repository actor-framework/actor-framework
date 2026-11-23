// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/tracking_resource.hpp"

namespace caf::test {

void* tracking_resource::do_allocate(size_t bytes, size_t alignment) {
  auto ptr = upstream_->allocate(bytes, alignment);
  ++allocations_;
  allocated_bytes_ += bytes;
  return ptr;
}

void tracking_resource::do_deallocate(void* ptr, size_t bytes,
                                      size_t alignment) {
  upstream_->deallocate(ptr, bytes, alignment);
  ++deallocations_;
  deallocated_bytes_ += bytes;
}

bool tracking_resource::do_is_equal(
  const std::pmr::memory_resource& other) const noexcept {
  return this == &other;
}

} // namespace caf::test
