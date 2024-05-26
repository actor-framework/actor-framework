// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf::intrusive {

/// Intrusive base for singly linked types that allows queues to use `T` with
/// dummy nodes.
template <class T>
struct singly_linked {
  // -- member types -----------------------------------------------------------

  /// The type for dummy nodes in singly linked lists.
  using node_type = singly_linked<T>;

  /// Type of the pointer connecting two singly linked nodes.
  using node_pointer = node_type*;

  // -- constructors, destructors, and assignment operators --------------------

  singly_linked() noexcept = default;

  explicit singly_linked(node_pointer n) noexcept : next(n) {
    // nop
  }

  singly_linked(const singly_linked&) = delete;

  singly_linked& operator=(const singly_linked&) = delete;

  // -- member variables -------------------------------------------------------

  /// Intrusive pointer to the next element.
  node_pointer next = nullptr;
};

} // namespace caf::intrusive
