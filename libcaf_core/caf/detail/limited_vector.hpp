/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_DETAIL_LIMITED_VECTOR_HPP
#define CAF_DETAIL_LIMITED_VECTOR_HPP

#include <cstddef>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <initializer_list>

#include "caf/config.hpp"

namespace caf {
namespace detail {

/*
 * A vector with a fixed maximum size (uses an array internally).
 * @warning This implementation is highly optimized for arithmetic types and
 *          does <b>not</b> call constructors or destructors.
 */
template <class T, size_t MaxSize>
class limited_vector {
 public:
  using value_type              = T;
  using size_type               = size_t;
  using difference_type         = ptrdiff_t;
  using reference               = value_type&;
  using const_reference         = const value_type&;
  using pointer                 = value_type*;
  using const_pointer           = const value_type*;
  using iterator                = pointer;
  using const_iterator          = const_pointer;
  using reverse_iterator        = std::reverse_iterator<iterator>;
  using const_reverse_iterator  = std::reverse_iterator<const_iterator>;

  limited_vector() : m_size(0) {
    // nop
  }

  limited_vector(size_t initial_size) : m_size(initial_size) {
    T tmp;
    std::fill_n(begin(), initial_size, tmp);
  }

  limited_vector(const limited_vector& other) : m_size(other.m_size) {
    std::copy(other.begin(), other.end(), begin());
  }

  limited_vector& operator=(const limited_vector& other) {
    resize(other.size());
    std::copy(other.begin(), other.end(), begin());
    return *this;
  }

  void resize(size_type s) {
    CAF_REQUIRE(s <= MaxSize);
    m_size = s;
  }

  limited_vector(std::initializer_list<T> init) : m_size(init.size()) {
    CAF_REQUIRE(init.size() <= MaxSize);
    std::copy(init.begin(), init.end(), begin());
  }

  void assign(size_type count, const_reference value) {
    resize(count);
    std::fill(begin(), end(), value);
  }

  template <class InputIterator>
  void assign(InputIterator first, InputIterator last,
              // dummy SFINAE argument
              typename std::iterator_traits<InputIterator>::pointer = 0) {
    auto dist = std::distance(first, last);
    CAF_REQUIRE(dist >= 0);
    resize(static_cast<size_t>(dist));
    std::copy(first, last, begin());
  }

  size_type size() const {
    return m_size;
  }

  size_type max_size() const {
    return MaxSize;
  }

  size_type capacity() const {
    return max_size() - size();
  }

  void clear() {
    m_size = 0;
  }

  bool empty() const {
    return m_size == 0;
  }

  bool full() const {
    return m_size == MaxSize;
  }

  void push_back(const_reference what) {
    CAF_REQUIRE(!full());
    m_data[m_size++] = what;
  }

  void pop_back() {
    CAF_REQUIRE(!empty());
    --m_size;
  }

  reference at(size_type pos) {
    CAF_REQUIRE(pos < m_size);
    return m_data[pos];
  }

  const_reference at(size_type pos) const {
    CAF_REQUIRE(pos < m_size);
    return m_data[pos];
  }

  reference operator[](size_type pos) {
    return at(pos);
  }

  const_reference operator[](size_type pos) const {
    return at(pos);
  }

  iterator begin() {
    return m_data;
  }

  const_iterator begin() const {
    return m_data;
  }

  const_iterator cbegin() const {
    return begin();
  }

  iterator end() {
    return begin() + m_size;
  }

  const_iterator end() const {
    return begin() + m_size;
  }

  const_iterator cend() const {
    return end();
  }

  reverse_iterator rbegin() {
    return reverse_iterator(end());
  }

  const_reverse_iterator rbegin() const {
    return reverse_iterator(end());
  }

  const_reverse_iterator crbegin() const {
    return rbegin();
  }

  reverse_iterator rend() {
    return reverse_iterator(begin());
  }

  const_reverse_iterator rend() const {
    return reverse_iterator(begin());
  }

  const_reverse_iterator crend() const {
    return rend();
  }

  reference front() {
    CAF_REQUIRE(!empty());
    return m_data[0];
  }

  const_reference front() const {
    CAF_REQUIRE(!empty());
    return m_data[0];
  }

  reference back() {
    CAF_REQUIRE(!empty());
    return m_data[m_size - 1];
  }

  const_reference back() const {
    CAF_REQUIRE(!empty());
    return m_data[m_size - 1];
  }

  T* data() {
    return m_data;
  }

  const T* data() const {
    return m_data;
  }

  template <class InputIterator>
  void insert(iterator pos, InputIterator first, InputIterator last) {
    CAF_REQUIRE(first <= last);
    auto num_elements = static_cast<size_t>(std::distance(first, last));
    if ((size() + num_elements) > MaxSize) {
      throw std::length_error("limited_vector::insert: too much elements");
    }
    if (pos == end()) {
      resize(size() + num_elements);
      std::copy(first, last, pos);
    }
    else {
      // move elements
      auto old_end = end();
      resize(size() + num_elements);
      std::copy_backward(pos, old_end, end());
      // insert new elements
      std::copy(first, last, pos);
    }
  }

 private:
  size_t m_size;
  T m_data[(MaxSize > 0) ? MaxSize : 1];
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_LIMITED_VECTOR_HPP
