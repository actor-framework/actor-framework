// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/inspector/json_writer.hpp"
#include "caf/placement_ptr.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace caf::inspector {

/// Factory for creating JSON writers.
class CAF_CORE_EXPORT json_writer_factory {
public:
  /// The minimum size required to construct a JSON writer.
  constexpr static size_t object_storage_size = 196;

  /// Storage for creating a JSON writer without heap allocation.
  struct object_storage {
    alignas(std::max_align_t) std::byte data[object_storage_size];
  };

  /// Smart pointer type for a heap-allocated JSON writer.
  using pointer_type = std::unique_ptr<json_writer>;

  /// Constructs a JSON writer in-place using the given storage.
  placement_ptr<json_writer> construct(std::span<std::byte> storage) const;

  /// Constructs a JSON writer in-place using the given storage.
  placement_ptr<json_writer> construct(object_storage& storage) const {
    return construct(storage.data);
  }

  /// Creates a new JSON writer on the heap.
  pointer_type create() const;
};

} // namespace caf::inspector
