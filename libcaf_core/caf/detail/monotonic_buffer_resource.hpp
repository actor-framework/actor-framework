// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <map>

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"

#ifdef CAF_CLANG
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wc99-extensions"
#elif defined(CAF_GCC)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
#elif defined(CAF_MSVC)
#  pragma warning(push)
#  pragma warning(disable : 4200)
#endif

namespace caf::detail {

/// Replacement for `std::pmr::monotonic_buffer_resource`, which sadly is not
/// available on all platforms CAF currenlty supports. This resource does not
/// support upstream resources and instead always uses `malloc` and `free`.
class CAF_CORE_EXPORT monotonic_buffer_resource {
public:
  // -- member types -----------------------------------------------------------

  /// A single block of memory.
  struct block {
    block* next;
    std::byte bytes[];
  };

  /// A bucket for storing multiple blocks.
  struct bucket {
    block* head = nullptr;
    std::byte* curr_pos = nullptr;
    std::byte* curr_end = nullptr;
    block* spare = nullptr;
    size_t block_size = 0;
  };

  template <class T>
  class allocator {
  public:
    using value_type = T;

    template <class U>
    struct rebind {
      using other = allocator<U>;
    };

    explicit allocator(monotonic_buffer_resource* mbr) : mbr_(mbr) {
      // nop
    }

    allocator() : mbr_(nullptr) {
      // nop
    }

    allocator(const allocator&) = default;

    allocator& operator=(const allocator&) = default;

    template <class U>
    allocator(const allocator<U>& other) : mbr_(other.resource()) {
      // nop
    }

    template <class U>
    allocator& operator=(const allocator<U>& other) {
      mbr_ = other.resource();
      return *this;
    }

    T* allocate(size_t n) {
      return static_cast<T*>(mbr_->allocate(sizeof(T) * n, alignof(T)));
    }

    constexpr void deallocate(void*, size_t) noexcept {
      // nop
    }

    constexpr auto resource() const noexcept {
      return mbr_;
    }

  private:
    monotonic_buffer_resource* mbr_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  monotonic_buffer_resource();

  ~monotonic_buffer_resource();

  // -- allocator interface ----------------------------------------------------

  /// Release all allocated memory to the OS even if no destructors were called
  /// for the allocated objects.
  void release();

  /// Reclaims all allocated memory (re-using it) even if no destructors were
  /// called for the allocated objects.
  void reclaim();

  /// Allocates memory.
  [[nodiscard]] void* allocate(size_t bytes,
                               size_t alignment = alignof(max_align_t));

  /// Fancy no-op.
  constexpr void deallocate(void*, size_t, size_t = alignof(max_align_t)) {
    // nop
  }

  /// Counts how many blocks currently exist in the bucket for @p alloc_size.
  size_t blocks(size_t alloc_size);

  /// Counts how many blocks currently exist in total.
  size_t blocks();

private:
  // Counts how many blocks exist in `where`.
  size_t blocks(bucket& where);

  // Gets the next free memory chunk.
  [[nodiscard]] void* do_alloc(bucket& from, size_t bytes, size_t alignment);

  // Adds another block to the bucket.
  void grow(bucket& what);

  // Select a bucket based on the allocation size.
  bucket& bucket_by_size(size_t alloc_size);

  // Sets all pointers back to nullptr.
  void reset(bucket& bkt);

  // Releases all memory in the bucket, leaving it in an invalid state.
  void release(bucket& bkt);

  // Shifts all blocks to `spare` list.
  void reclaim(bucket& bkt);

  // Objects of size <= 64 bytes.
  bucket small_;

  // Objects of size <= 512 bytes.
  bucket medium_;

  // Objects of various sizes > 512 bytes.
  std::map<size_t, bucket> var_;
};

template <class T, class U>
bool operator==(monotonic_buffer_resource::allocator<T> x,
                monotonic_buffer_resource::allocator<U> y) {
  return x.resource() == y.resource();
}

template <class T, class U>
bool operator!=(monotonic_buffer_resource::allocator<T> x,
                monotonic_buffer_resource::allocator<U> y) {
  return x.resource() != y.resource();
}

} // namespace caf::detail

#ifdef CAF_CLANG
#  pragma clang diagnostic pop
#elif defined(CAF_GCC)
#  pragma GCC diagnostic pop
#elif defined(CAF_MSVC)
#  pragma warning(pop)
#endif
