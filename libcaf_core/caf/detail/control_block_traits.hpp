// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/aligned_alloc.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/detail/critical.hpp"
#include "caf/detail/memory_interface.hpp"
#include "caf/detail/panic.hpp"
#include "caf/raise_error.hpp"

#include <cstddef>
#include <cstdint>
#include <new>

namespace caf::detail {

/// Traits and utility functions for control blocks.
template <class ControlBlock>
class control_block_traits {
public:
  using managed_type = typename ControlBlock::managed_type;

  /// Returns a pointer to the managed object from a control block pointer.
  static managed_type* managed_ptr(const ControlBlock* ptr) noexcept {
    return reinterpret_cast<managed_type*>(to_intptr(ptr)
                                           + ControlBlock::allocation_size);
  }

  /// Returns a pointer to the control block from a managed object pointer.
  static ControlBlock* ctrl_ptr(const managed_type* ptr) noexcept {
    return reinterpret_cast<ControlBlock*>(to_intptr(ptr)
                                           - ControlBlock::allocation_size);
  }

  template <class ManagedType>
  static void* allocate() {
    static_assert(std::is_base_of_v<managed_type, ManagedType>);
    static_assert(ControlBlock::memory_interface
                  == memory_interface::aligned_alloc_and_free);
    constexpr size_t alloc_size = ControlBlock::allocation_size
                                  + sizeof(ManagedType);
    auto* mem = aligned_alloc(ControlBlock::alignment, alloc_size);
    if (mem == nullptr) {
      CAF_RAISE_ERROR(std::bad_alloc, "failed to allocate aligned memory");
    }
    return mem;
  }

  template <class... Args>
  static ControlBlock* construct_ctrl(void* mem, Args&&... args) {
    auto* ptr = new (mem) ControlBlock(std::forward<Args>(args)...);
#ifdef CAF_ENABLE_RUNTIME_CHECKS
    if (to_intptr(ptr) != to_intptr(mem)) {
      critical("constructed control block at misaligned address");
    }
#endif
    return ptr;
  }

  template <class ManagedType, class... Args>
  static ManagedType* construct_managed(void* block, Args&&... args) {
    static_assert(std::is_base_of_v<managed_type, ManagedType>);
    auto* mem = reinterpret_cast<std::byte*>(block)
                + ControlBlock::allocation_size;
    auto* ptr = new (mem) ManagedType(std::forward<Args>(args)...);
#ifdef CAF_ENABLE_RUNTIME_CHECKS
    if (to_intptr(ptr) != to_intptr(mem)) {
      critical("constructed control block at misaligned address");
    }
    if constexpr (!std::is_same_v<ManagedType, managed_type>) {
      const managed_type* base_ptr = ptr;
      if (to_intptr(base_ptr) != to_intptr(ptr)) {
        critical("incompatible memory layout between base and derived class");
      }
    }
#endif
    return ptr;
  }

private:
  static constexpr intptr_t to_intptr(const void* ptr) noexcept {
    return reinterpret_cast<intptr_t>(ptr);
  }
};

} // namespace caf::detail
