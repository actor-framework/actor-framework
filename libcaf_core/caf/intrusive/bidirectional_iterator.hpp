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

#ifndef CAF_INTRUSIVE_BIDIRECTIONAL_ITERATOR_HPP
#define CAF_INTRUSIVE_BIDIRECTIONAL_ITERATOR_HPP

#include <cstddef>
#include <iterator>
#include <type_traits>

#include "caf/intrusive/doubly_linked.hpp"

namespace caf {
namespace intrusive {

/// Intrusive base for doubly linked types that allows queues to use `T` with
/// node nodes.
template <class T>
class bidirectional_iterator {
public:
  // -- member types -----------------------------------------------------------

  using difference_type = std::ptrdiff_t;

  using value_type = T;

  using pointer = value_type*;

  using const_pointer = const value_type*;

  using reference = value_type&;

  using const_reference = const value_type&;

  using node_type =
    typename std::conditional<
      std::is_const<T>::value,
      const typename T::node_type,
      typename T::node_type
    >::type;

  using node_pointer = node_type*;

  using iterator_category = std::bidirectional_iterator_tag;

  // -- member variables -------------------------------------------------------

  node_pointer ptr;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr bidirectional_iterator(node_pointer init = nullptr) : ptr(init) {
    // nop
  }

  bidirectional_iterator(const bidirectional_iterator&) = default;

  bidirectional_iterator& operator=(const bidirectional_iterator&) = default;

  // -- convenience functions --------------------------------------------------

  bidirectional_iterator next() {
    return ptr->next;
  }

  // -- operators --------------------------------------------------------------

  bidirectional_iterator& operator++() {
    ptr = promote(ptr->next);
    return *this;
  }

  bidirectional_iterator operator++(int) {
    bidirectional_iterator res = *this;
    ptr = promote(ptr->next);
    return res;
  }

  bidirectional_iterator& operator--() {
    ptr = ptr->prev;
    return *this;
  }

  bidirectional_iterator operator--(int) {
    bidirectional_iterator res = *this;
    ptr = promote(ptr->prev);
    return res;
  }

  reference operator*() {
    return *promote(ptr);
  }

  const_reference operator*() const {
    return *promote(ptr);
  }

  pointer operator->() {
    return promote(ptr);
  }

  const pointer operator->() const {
    return promote(ptr);
  }
};

/// @relates bidirectional_iterator
template <class T>
bool operator==(const bidirectional_iterator<T>& x,
                const bidirectional_iterator<T>& y) {
  return x.ptr == y.ptr;
}

/// @relates bidirectional_iterator
template <class T>
bool operator!=(const bidirectional_iterator<T>& x,
                const bidirectional_iterator<T>& y) {
  return x.ptr != y.ptr;
}

} // namespace intrusive
} // namespace caf

#endif // CAF_INTRUSIVE_BIDIRECTIONAL_ITERATOR_HPP
