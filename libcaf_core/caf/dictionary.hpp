/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <algorithm>
#include <iterator>
#include <map>
#include <string>

#include "caf/string_view.hpp"

namespace caf {

/// Maps strings to values of type `V`, but unlike `std::map<std::string, V>`
/// accepts `string_view` for looking up keys efficiently.
template <class V>
class dictionary {
public:
  // -- member types ----------------------------------------------------------

  using map_type = std::map<std::string, V>;

  using key_type = typename map_type::key_type;

  using mapped_type = typename map_type::mapped_type;

  using value_type = typename map_type::value_type;

  using allocator_type = typename map_type::allocator_type;

  using size_type = typename map_type::size_type;

  using difference_type = typename map_type::difference_type;

  using reference = typename map_type::reference;

  using const_reference = typename map_type::const_reference;

  using pointer = typename map_type::pointer;

  using const_pointer = typename map_type::const_pointer;

  using iterator = typename map_type::iterator;

  using const_iterator = typename map_type::const_iterator;

  using reverse_iterator = typename map_type::reverse_iterator;

  using const_reverse_iterator = typename map_type::const_reverse_iterator;

  using iterator_bool_pair = std::pair<iterator, bool>;

  struct mapped_type_less {
    inline bool operator()(const value_type& x, string_view y) const {
      return x.first < y;
    }

    inline bool operator()(const value_type& x, const value_type& y) const {
      return x.first < y.first;
    }

    inline bool operator()(string_view x, const value_type& y) const {
      return x < y.first;
    }
  };

  // -- constructors, destructors, and assignment operators -------------------

  dictionary() = default;

  dictionary(dictionary&&) = default;

  dictionary(const dictionary&) = default;

  dictionary(std::initializer_list<value_type> xs) : xs_(xs) {
    // nop
  }

  template <class InputIterator>
  dictionary(InputIterator first, InputIterator last) : xs_(first, last) {
    // nop
  }

  dictionary& operator=(dictionary&&) = default;

  dictionary& operator=(const dictionary&) = default;

  // -- iterator access --------------------------------------------------------

  iterator begin() noexcept {
    return xs_.begin();
  }

  const_iterator begin() const noexcept {
    return xs_.begin();
  }

  const_iterator cbegin() const noexcept {
    return begin();
  }

  reverse_iterator rbegin() noexcept {
    return xs_.rbegin();
  }

  const_reverse_iterator rbegin() const noexcept {
    return xs_.rbegin();
  }

  const_reverse_iterator crbegin() const noexcept {
    return rbegin();
  }

  iterator end() noexcept {
    return xs_.end();
  }

  const_iterator end() const noexcept {
    return xs_.end();
  }

  const_iterator cend() const noexcept {
    return end();
  }

  reverse_iterator rend() noexcept {
    return xs_.rend();
  }

  const_reverse_iterator rend() const noexcept {
    return xs_.rend();
  }

  const_reverse_iterator crend() const noexcept {
    return rend();
  }

  // -- size -------------------------------------------------------------------

  bool empty() const noexcept {
    return xs_.empty();
  }

  size_type size() const noexcept {
    return xs_.size();
  }

  // -- access to members ------------------------------------------------------

  /// Gives raw access to the underlying container.
  map_type& container() noexcept {
    return xs_;
  }

  /// Gives raw access to the underlying container.
  const map_type& container() const noexcept {
    return xs_;
  }

  // -- modifiers -------------------------------------------------------------

  void clear() noexcept {
    return xs_.clear();
  }

  void swap(dictionary& other) {
    xs_.swap(other.xs_);
  }

  // -- insertion --------------------------------------------------------------

  template <class K, class T>
  iterator_bool_pair emplace(K&& key, T&& value) {
    auto i = lower_bound(key);
    if (i == end())
      return xs_.emplace(copy(std::forward<K>(key)), V{std::forward<T>(value)});
    if (i->first == key)
      return {i, false};
    return {xs_.emplace_hint(i, copy(std::forward<K>(key)),
                             V{std::forward<T>(value)}),
            true};
  }

  iterator_bool_pair insert(value_type kvp) {
    return emplace(kvp.first, std::move(kvp.second));
  }

  iterator insert(iterator hint, value_type kvp) {
    return emplace_hint(hint, kvp.first, std::move(kvp.second));
  }

  template <class T>
  iterator_bool_pair insert(string_view key, T&& value) {
    return emplace(key, V{std::forward<T>(value)});
  }

  template <class K, class T>
  iterator emplace_hint(iterator hint, K&& key, T&& value) {
    if (hint == end() || hint->first > key)
      return xs_.emplace(copy(std::forward<K>(key)), V{std::forward<T>(value)})
             .first;
    if (hint->first == key)
      return hint;
    return xs_.emplace_hint(hint, copy(std::forward<K>(key)),
                            V{std::forward<T>(value)});
  }

  template <class T>
  iterator insert(iterator hint, string_view key, T&& value) {
    return emplace_hint(hint, key, std::forward<T>(value));
  }

  void insert(const_iterator first, const_iterator last) {
    xs_.insert(first, last);
  }

  template <class T>
  iterator_bool_pair insert_or_assign(string_view key, T&& value) {
    auto i = lower_bound(key);
    if (i == end())
      return xs_.emplace(copy(key), V{std::forward<T>(value)});
    if (i->first == key) {
      i->second = V{std::forward<T>(value)};
      return {i, false};
    }
    return {xs_.emplace_hint(i, copy(key), V{std::forward<T>(value)}), true};
  }

  template <class T>
  iterator insert_or_assign(iterator hint, string_view key, T&& value) {
    if (hint == end() || hint->first > key)
      return insert_or_assign(key, std::forward<T>(value)).first;
    hint = lower_bound(hint, key);
    if (hint != end() && hint->first == key) {
      hint->second = std::forward<T>(value);
      return hint;
    }
    return xs_.emplace_hint(hint, copy(key), V{std::forward<T>(value)});
  }

  // -- lookup -----------------------------------------------------------------

  bool contains(string_view key) const noexcept {
    auto i = lower_bound(key);
    return !(i == end() || i->first != key);
  }

  size_t count(string_view key) const noexcept {
    return contains(key) ? 1u : 0u;
  }

  iterator find(string_view key) noexcept {
    auto i = lower_bound(key);
    return i != end() && i->first == key ? i : end();
  }

  const_iterator find(string_view key) const noexcept {
    auto i = lower_bound(key);
    return i != end() && i->first == key ? i : end();
  }

  iterator lower_bound(string_view key) {
    return lower_bound(begin(), key);
  }

  const_iterator lower_bound(string_view key) const {
    return lower_bound(begin(), key);
  }

  iterator upper_bound(string_view key) {
    mapped_type_less cmp;
    return std::upper_bound(begin(), end(), key, cmp);
  }

  const_iterator upper_bound(string_view key) const {
    mapped_type_less cmp;
    return std::upper_bound(begin(), end(), key, cmp);
  }

  // -- element access ---------------------------------------------------------

  mapped_type& operator[](string_view key) {
    return insert(key, mapped_type{}).first->second;
  }

private:
  iterator lower_bound(iterator from, string_view key) {
    mapped_type_less cmp;
    return std::lower_bound(from, end(), key, cmp);
  }

  const_iterator lower_bound(const_iterator from, string_view key) const {
    mapped_type_less cmp;
    return std::lower_bound(from, end(), key, cmp);
  }

  template <size_t N>
  static inline std::string copy(const char (&str)[N]) {
    return std::string{str};
  }

  // Copies the content of `str` into a new string.
  static inline std::string copy(string_view str) {
    return std::string{str.begin(), str.end()};
  }

  // Moves the content of `str` into a new string.
  static inline std::string copy(std::string str) {
    return str;
  }

  map_type xs_;
};

// @relates dictionary
template <class T>
bool operator==(const dictionary<T>& xs, const dictionary<T>& ys) {
  return xs.container() == ys.container();
}

// @relates dictionary
template <class T>
bool operator!=(const dictionary<T>& xs, const dictionary<T>& ys) {
  return xs.container() != ys.container();
}

// @relates dictionary
template <class T>
bool operator<(const dictionary<T>& xs, const dictionary<T>& ys) {
  return xs.container() < ys.container();
}

// @relates dictionary
template <class T>
bool operator<=(const dictionary<T>& xs, const dictionary<T>& ys) {
  return xs.container() <= ys.container();
}

// @relates dictionary
template <class T>
bool operator>(const dictionary<T>& xs, const dictionary<T>& ys) {
  return xs.container() > ys.container();
}

// @relates dictionary
template <class T>
bool operator>=(const dictionary<T>& xs, const dictionary<T>& ys) {
  return xs.container() >= ys.container();
}

} // namespace caf
