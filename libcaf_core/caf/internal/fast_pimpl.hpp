// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/assert.hpp"

#include <cstddef>
#include <new>

namespace caf::internal {

/// Base class for implementing implementations with a fast pimpl idiom.
template <class T>
struct fast_pimpl {
  /// Casts a pointer to the storage to a reference to the implementation.
  static T& cast(std::byte* storage) {
    return *reinterpret_cast<T*>(storage);
  }

  /// Casts a pointer to the storage to a reference to the implementation.
  static const T& cast(const std::byte* storage) {
    return *reinterpret_cast<const T*>(storage);
  }

  /// Constructs the implementation in the given storage.
  template <size_t N, class... Args>
  static void construct(std::byte (&storage)[N], Args&&... args) {
    static_assert(sizeof(T) <= N);
    [[maybe_unused]] auto* obj = new (storage) T(std::forward<Args>(args)...);
#ifdef CAF_ENABLE_RUNTIME_CHECKS
    // Make sure reinterpret_cast on the storage address is safe.
    auto storage_addr = reinterpret_cast<intptr_t>(storage);
    auto obj_addr = reinterpret_cast<intptr_t>(obj);
    CAF_ASSERT(storage_addr == obj_addr);
#endif
  }

  /// Destructs the implementation in the given storage.
  static void destruct(std::byte* storage) {
    cast(storage).~T();
  }
};

} // namespace caf::internal
