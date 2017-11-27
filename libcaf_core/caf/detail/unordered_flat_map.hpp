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

#ifndef CAF_DETAIL_UNORDERED_FLAT_MAP_HPP
#define CAF_DETAIL_UNORDERED_FLAT_MAP_HPP

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <functional>

#include "caf/detail/comparable.hpp"

namespace caf {
namespace detail {

/// A map abstraction with an unsorted `std::vector` providing `O(n)` lookup.
template <class Key, class T,
          class Allocator = std::allocator<std::pair<Key, T>>>
class unordered_flat_map {
public:
  // -- member types ----------------------------------------------------------

  using key_type = Key;

  using mapped_type = T;

  using value_type = std::pair<Key, T>;

  using vector_type = std::vector<value_type, Allocator>;

  using allocator_type = typename vector_type::allocator_type;

  using size_type = typename vector_type::size_type;

  using difference_type = typename vector_type::difference_type;

  using reference = typename vector_type::reference;

  using const_reference = typename vector_type::const_reference;

  using pointer = typename vector_type::pointer;

  using const_pointer = typename vector_type::const_pointer;

  using iterator = typename vector_type::iterator;

  using const_iterator = typename vector_type::const_iterator;

  using reverse_iterator = typename vector_type::reverse_iterator;

  using const_reverse_iterator = typename vector_type::const_reverse_iterator;

  // -- constructors, destructors, and assignment operators -------------------

  unordered_flat_map() = default;

  unordered_flat_map(std::initializer_list<value_type> l) : xs_(l) {
    // nop
  }

  template <class InputIterator>
  unordered_flat_map(InputIterator first, InputIterator last)
      : xs_(first, last) {
    // nop
  }

  // -- non-const begin iterators ---------------------------------------------

  iterator begin() {
    return xs_.begin();
  }

  reverse_iterator rbegin() {
    return xs_.rbegin();
  }

  // -- const begin iterators -------------------------------------------------

  const_iterator begin() const {
    return xs_.begin();
  }

  const_iterator cbegin() const {
    return xs_.cbegin();
  }

  const_reverse_iterator rbegin() const {
    return xs_.rbegin();
  }

  // -- non-const end iterators -----------------------------------------------

  iterator end() {
    return xs_.end();
  }

  reverse_iterator rend() {
    return xs_.rend();
  }

  // -- const end iterators ---------------------------------------------------

  const_iterator end() const {
    return xs_.end();
  }

  const_iterator cend() const {
    return xs_.end();
  }

  const_reverse_iterator rend() const {
    return xs_.rend();
  }

  // -- capacity --------------------------------------------------------------

  bool empty() const {
    return xs_.empty();
  }

  size_type size() const {
    return xs_.size();
  }

  void reserve(size_type count) {
    xs_.reserve(count);
  }

  void shrink_to_fit() {
    xs_.shrink_to_fit();
  }

  // -- modifiers -------------------------------------------------------------

  void clear() {
    return xs_.clear();
  }

  std::pair<iterator, bool> insert(value_type x) {
    return insert(end(), std::move(x));
  }

  std::pair<iterator, bool> insert(iterator hint, value_type x) {
    return insert(static_cast<const_iterator>(hint), std::move(x));
  }

  std::pair<iterator, bool> insert(const_iterator hint, value_type x) {
    auto i = find(x.first);
    if (i == end())
      return {xs_.insert(hint, std::move(x)), true};
    return {i, false};
  }

  template <class InputIterator>
  void insert(InputIterator first, InputIterator last) {
    while (first != last)
      insert(*first++);
  }

  template <class... Ts>
  std::pair<iterator, bool> emplace(Ts&&... xs) {
    return emplace_hint(end(), std::forward<Ts>(xs)...);
  }

  template <class... Ts>
  std::pair<iterator, bool> emplace_hint(const_iterator hint, Ts&&... xs) {
    return insert(hint, value_type(std::forward<Ts>(xs)...));
  }

  iterator erase(const_iterator i) {
    return xs_.erase(i);
  }

  iterator erase(const_iterator first, const_iterator last) {
    return xs_.erase(first, last);
  }

  size_type erase(const key_type& x) {
    auto pred = [&](const value_type& y) {
      return x == y.first;
    };
    auto i = std::remove_if(begin(), end(), pred);
    if (i == end())
      return 0;
    erase(i);
    return 1;
  }

  void swap(unordered_flat_map& other) {
    xs_.swap(other);
  }

  // -- lookup ----------------------------------------------------------------

  template <class K>
  mapped_type& at(const K& key) {
    auto i = find(key);
    if (i == end())
      throw std::out_of_range{"caf::detail::unordered_flat_map::at"};
    return i->second;
  }

  template <class K>
  const mapped_type& at(const K& key) const {
    auto i = find(key);
    if (i == end())
      throw std::out_of_range{"caf::detail::unordered_flat_map::at"};
    return i->second;
  }

  mapped_type& operator[](const key_type& key) {
    auto i = find(key);
    if (i != end())
      return i->second;
    return xs_.insert(i, value_type{key, mapped_type{}})->second;
  }

  template <class K>
  iterator find(const K& x) {
    auto pred = [&](const value_type& y) {
      return x == y.first;
    };
    return std::find_if(xs_.begin(), xs_.end(), pred);
  }

  template <class K>
  const_iterator find(const K& x) const {
    auto pred = [&](const value_type& y) {
      return x == y.first;
    };
    return std::find_if(xs_.begin(), xs_.end(), pred);
  }

  template <class K>
  size_type count(const K& x) const {
    return find(x) == end() ? 0 : 1;
  }

private:
  vector_type xs_;
};

template <class K, class T, class A>
bool operator==(const unordered_flat_map<K, T, A>& xs,
                const unordered_flat_map<K, T, A>& ys) {
  return xs.size() == ys.size() && std::equal(xs.begin(), xs.end(), ys.begin());
}

template <class K, class T, class A>
bool operator!=(const unordered_flat_map<K, T, A>& xs,
                const unordered_flat_map<K, T, A>& ys) {
  return !(xs == ys);
}

template <class K, class T, class A>
bool operator<(const unordered_flat_map<K, T, A>& xs,
               const unordered_flat_map<K, T, A>& ys) {
  return std::lexicographical_compare(xs.begin(), xs.end(),
                                      ys.begin(), ys.end());
}

template <class K, class T, class A>
bool operator>=(const unordered_flat_map<K, T, A>& xs,
                const unordered_flat_map<K, T, A>& ys) {
  return !(xs < ys);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_UNORDERED_FLAT_MAP_HPP

