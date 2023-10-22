// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/type_id.hpp"

#include <cstdlib>

namespace caf::detail {

class CAF_CORE_EXPORT type_id_list_builder {
public:
  // -- constants --------------------------------------------------------------

  /// The number of elements that we allocate at once.
  static constexpr size_t block_size = 8;

  // -- constructors, destructors, and assignment operators --------------------

  /// Constructs an empty type ID list builder.
  type_id_list_builder() = default;

  /// Constructs an empty type ID list builder that can hold at least
  /// `size_hint` elements without reallocation.
  explicit type_id_list_builder(size_t size_hint);

  ~type_id_list_builder();

  // -- modifiers --------------------------------------------------------------

  /// Appends `id` to the type ID list.
  void push_back(type_id_t id);

  /// Removes all elements from the type ID list.
  void clear() noexcept;

  // -- properties -------------------------------------------------------------

  /// Returns the number of elements in the type ID list.
  size_t size() const noexcept;

  /// Returns the number of element slots reserved in the type ID list.
  /// @note The capacity is always a multiple of @ref block_size.
  /// @note The capacity contains a dummy element at the beginning. Hence, the
  ///       actual number of elements that can be stored is `capacity() - 1`.
  size_t capacity() const noexcept {
    return capacity_;
  }

  /// Returns the element at position `index`.
  /// @pre `index < size()`
  type_id_t operator[](size_t index) const noexcept;

  // -- conversions ------------------------------------------------------------

  /// Converts the internal buffer to a @ref type_id_list and transfers
  /// ownership of the data to the new list.
  /// @note The builder is in a defined-but-invalid state after this operation.
  ///       Calling `clear()` will restore the builder to a valid state.
  type_id_list move_to_list() noexcept;

  /// Converts the internal buffer to a @ref type_id_list and returns it.
  type_id_list copy_to_list() const;

private:
  void reserve(size_t new_capacity);

  size_t size_ = 0;
  size_t capacity_ = 0;
  type_id_t* storage_ = nullptr;
};

} // namespace caf::detail
