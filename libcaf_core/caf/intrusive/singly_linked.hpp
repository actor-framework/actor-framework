/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

namespace caf {
namespace intrusive {

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

/// Casts a node type to its value type.
template <class T>
T* promote(singly_linked<T>* ptr) {
  return static_cast<T*>(ptr);
}

/// Casts a node type to its value type.
template <class T>
const T* promote(const singly_linked<T>* ptr) {
  return static_cast<const T*>(ptr);
}

} // namespace intrusive
} // namespace caf

