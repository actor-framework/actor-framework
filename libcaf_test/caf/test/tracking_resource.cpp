// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/tracking_resource.hpp"

namespace caf::test {

tracking_resource::tracking_resource(std::pmr::memory_resource* upstream)
  : upstream_(upstream) {
  // nop
}

std::size_t tracking_resource::allocations() const noexcept {
  return allocations_;
}

std::size_t tracking_resource::deallocations() const noexcept {
  return deallocations_;
}

std::size_t tracking_resource::outstanding_allocations() const noexcept {
  return allocations_ - deallocations_;
}

std::size_t tracking_resource::outstanding_bytes() const noexcept {
  return allocated_bytes_ - deallocated_bytes_;
}

void* tracking_resource::do_allocate(std::size_t bytes, std::size_t alignment) {
  auto ptr = upstream_->allocate(bytes, alignment);
  ++allocations_;
  allocated_bytes_ += bytes;
  return ptr;
}

void tracking_resource::do_deallocate(void* ptr, std::size_t bytes,
                                      std::size_t alignment) {
  upstream_->deallocate(ptr, bytes, alignment);
  ++deallocations_;
  deallocated_bytes_ += bytes;
}

bool tracking_resource::do_is_equal(
  const std::pmr::memory_resource& other) const noexcept {
  return this == &other;
}

} // namespace caf::test
