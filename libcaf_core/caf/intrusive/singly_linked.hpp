// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

  singly_linked(node_pointer n = nullptr) : next(n) {
    // nop
  }

  // -- member variables -------------------------------------------------------

  /// Intrusive pointer to the next element.
  node_pointer next;
};

} // namespace caf::intrusive
