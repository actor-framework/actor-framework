// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/monotonic_buffer_resource.hpp"

#include <limits>
#include <memory>

#include "caf/raise_error.hpp"

namespace caf::detail {

monotonic_buffer_resource::monotonic_buffer_resource() {
  // 8kb blocks for the small and medium sized buckets.
  small_.block_size = 8 * 1024;
  medium_.block_size = 8 * 1024;
}

monotonic_buffer_resource::~monotonic_buffer_resource() {
  release(small_);
  release(medium_);
  for (auto& kvp : var_)
    release(kvp.second);
}

void monotonic_buffer_resource::release() {
  release(small_);
  reset(small_);
  release(medium_);
  reset(medium_);
  for (auto& kvp : var_)
    release(kvp.second);
  var_.clear();
}

void monotonic_buffer_resource::reclaim() {
  // Only reclaim the small and medium buffers. They have a high chance of
  // actually reducing future heap allocations. Because of the relatively small
  // bucket sizes, the variable buckets have a higher change of not producing
  // 'hits' in future runs. We can get smarter about managing larger
  // allocations, but ultimately this custom memory resource implementation is a
  // placeholder until we can use the new `std::pmr` utilities. So as long as
  // performance is "good enough", we keep our implementation simple and err on
  // the side of caution for now.
  reclaim(small_);
  reclaim(medium_);
  for (auto& kvp : var_)
    release(kvp.second);
  var_.clear();
}

void* monotonic_buffer_resource::allocate(size_t bytes, size_t alignment) {
  return do_alloc(bucket_by_size(bytes), bytes, alignment);
}

size_t monotonic_buffer_resource::blocks(size_t alloc_size) {
  return blocks(bucket_by_size(alloc_size));
}

size_t monotonic_buffer_resource::blocks() {
  auto result = blocks(small_) + blocks(medium_);
  for (auto& kvp : var_)
    result += blocks(kvp.second);
  return result;
}

size_t monotonic_buffer_resource::blocks(bucket& where) {
  size_t result = 0;
  for (auto ptr = where.head; ptr != nullptr; ptr = ptr->next)
    ++result;
  for (auto ptr = where.spare; ptr != nullptr; ptr = ptr->next)
    ++result;
  return result;
}

void* monotonic_buffer_resource::do_alloc(bucket& from, size_t bytes,
                                          size_t alignment) {
  for (;;) {
    if (from.curr_pos != nullptr) {
      auto result = static_cast<void*>(from.curr_pos);
      auto space = static_cast<size_t>(from.curr_end - from.curr_pos);
      if (std::align(alignment, bytes, result, space)) {
        from.curr_pos = static_cast<std::byte*>(result) + bytes;
        return result;
      }
    }
    // Try again after allocating more storage.
    grow(from);
  }
}

void monotonic_buffer_resource::grow(bucket& what) {
  auto init = [&what](block* blk) {
    blk->next = what.head;
    what.head = blk;
    what.curr_pos = blk->bytes;
    what.curr_end = reinterpret_cast<std::byte*>(blk) + what.block_size;
  };
  if (what.spare != nullptr) {
    auto blk = what.spare;
    what.spare = blk->next;
    init(blk);
  } else if (auto ptr = malloc(what.block_size)) {
    init(static_cast<block*>(ptr));
  } else {
    CAF_RAISE_ERROR(std::bad_alloc, "monotonic_buffer_resource");
  }
}

monotonic_buffer_resource::bucket&
monotonic_buffer_resource::bucket_by_size(size_t alloc_size) {
  constexpr auto max_alloc_size = std::numeric_limits<size_t>::max()
                                  - sizeof(block) - alignof(max_align_t);
  auto var_bucket = [this](size_t key, size_t block_size) -> bucket& {
    if (auto i = var_.find(key); i != var_.end()) {
      return i->second;
    } else {
      bucket tmp;
      tmp.block_size = block_size;
      return var_.emplace(key, tmp).first->second;
    }
  };
  if (alloc_size <= 64) {
    return small_;
  } else if (alloc_size <= 512) {
    return medium_;
  } else if (alloc_size <= 1'048'576) { // 1MB
    // Align on 1kb and reserve memory for up to four elements.
    auto bucket_key = ((alloc_size / 1024) + 1) * 1024;
    return var_bucket(bucket_key, bucket_key * 4);
  } else if (alloc_size <= max_alloc_size) {
    // Fall back to individual allocations.
    return var_bucket(alloc_size,
                      alloc_size + sizeof(block) + alignof(max_align_t));
  } else {
    CAF_RAISE_ERROR(std::bad_alloc, "monotonic_buffer_resource");
  }
}

void monotonic_buffer_resource::reset(bucket& bkt) {
  bkt.head = nullptr;
  bkt.curr_pos = nullptr;
  bkt.curr_end = nullptr;
  bkt.spare = nullptr;
}

void monotonic_buffer_resource::release(bucket& bkt) {
  for (auto ptr = bkt.head; ptr != nullptr;) {
    auto blk = ptr;
    ptr = ptr->next;
    free(blk);
  }
  for (auto ptr = bkt.spare; ptr != nullptr;) {
    auto blk = ptr;
    ptr = ptr->next;
    free(blk);
  }
}

void monotonic_buffer_resource::reclaim(bucket& bkt) {
  for (auto ptr = bkt.head; ptr != nullptr;) {
    auto blk = ptr;
    ptr = ptr->next;
    blk->next = bkt.spare;
    bkt.spare = blk;
  }
  bkt.head = nullptr;
  bkt.curr_pos = nullptr;
  bkt.curr_end = nullptr;
}

} // namespace caf::detail
