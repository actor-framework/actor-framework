// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace caf::intrusive {

// A forward iterator for intrusive lists.
template <class T>
class forward_iterator {
public:
  // -- member types -----------------------------------------------------------

  using difference_type = std::ptrdiff_t;

  using value_type = T;

  using pointer = value_type*;

  using reference = value_type&;

  using node_type
    = std::conditional_t<std::is_const_v<T>,
                         const typename std::remove_const_t<T>::node_type,
                         typename T::node_type>;

  using node_pointer = node_type*;

  using iterator_category = std::forward_iterator_tag;

  // -- member variables -------------------------------------------------------

  node_pointer ptr = nullptr;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr forward_iterator() noexcept = default;

  constexpr explicit forward_iterator(node_pointer init) noexcept : ptr(init) {
    // nop
  }

  forward_iterator(const forward_iterator&) noexcept = default;

  forward_iterator& operator=(const forward_iterator&) noexcept = default;

  // -- convenience functions --------------------------------------------------

  forward_iterator next() const {
    return forward_iterator{ptr->next};
  }

  // -- operators --------------------------------------------------------------

  forward_iterator& operator++() {
    ptr = ptr->next;
    return *this;
  }

  forward_iterator operator++(int) {
    forward_iterator res = *this;
    ptr = ptr->next;
    return res;
  }

  reference operator*() const {
    return *promote(ptr);
  }

  pointer operator->() const {
    return promote(ptr);
  }

private:
  // -- static utility functions -----------------------------------------------

  /// Casts a node type to its value type.
  static pointer promote(node_pointer ptr) noexcept {
    return static_cast<pointer>(ptr);
  }
};

/// @relates forward_iterator
template <class T>
bool operator==(forward_iterator<T> x, forward_iterator<T> y) noexcept {
  return x.ptr == y.ptr;
}

/// @relates forward_iterator
template <class T>
bool operator!=(forward_iterator<T> x, forward_iterator<T> y) noexcept {
  return x.ptr != y.ptr;
}

} // namespace caf::intrusive
