/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

// A vector with a fixed maximum size (uses an array internally).
// @warning This implementation is highly optimized for arithmetic types and
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

  limited_vector() : size_(0) {
    // nop
  }

  explicit limited_vector(size_t initial_size) : size_(initial_size) {
    T tmp;
    std::fill_n(begin(), initial_size, tmp);
  }

  limited_vector(const limited_vector& other) : size_(other.size_) {
    std::copy(other.begin(), other.end(), begin());
  }

  limited_vector& operator=(const limited_vector& other) {
    resize(other.size());
    std::copy(other.begin(), other.end(), begin());
    return *this;
  }

  void resize(size_type s) {
    CAF_ASSERT(s <= MaxSize);
    size_ = s;
  }

  explicit limited_vector(std::initializer_list<T> init) : size_(init.size()) {
    CAF_ASSERT(init.size() <= MaxSize);
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
    CAF_ASSERT(dist >= 0);
    resize(static_cast<size_t>(dist));
    std::copy(first, last, begin());
  }

  size_type size() const {
    return size_;
  }

  size_type max_size() const {
    return MaxSize;
  }

  size_type capacity() const {
    return max_size() - size();
  }

  void clear() {
    size_ = 0;
  }

  bool empty() const {
    return size_ == 0;
  }

  bool full() const {
    return size_ == MaxSize;
  }

  void push_back(const_reference what) {
    CAF_ASSERT(! full());
    data_[size_++] = what;
  }

  void pop_back() {
    CAF_ASSERT(! empty());
    --size_;
  }

  reference at(size_type pos) {
    CAF_ASSERT(pos < size_);
    return data_[pos];
  }

  const_reference at(size_type pos) const {
    CAF_ASSERT(pos < size_);
    return data_[pos];
  }

  reference operator[](size_type pos) {
    return at(pos);
  }

  const_reference operator[](size_type pos) const {
    return at(pos);
  }

  iterator begin() {
    return data_;
  }

  const_iterator begin() const {
    return data_;
  }

  const_iterator cbegin() const {
    return begin();
  }

  iterator end() {
    return begin() + size_;
  }

  const_iterator end() const {
    return begin() + size_;
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
    CAF_ASSERT(! empty());
    return data_[0];
  }

  const_reference front() const {
    CAF_ASSERT(! empty());
    return data_[0];
  }

  reference back() {
    CAF_ASSERT(! empty());
    return data_[size_ - 1];
  }

  const_reference back() const {
    CAF_ASSERT(! empty());
    return data_[size_ - 1];
  }

  T* data() {
    return data_;
  }

  const T* data() const {
    return data_;
  }

  template <class InputIterator>
  void insert(iterator pos, InputIterator first, InputIterator last) {
    CAF_ASSERT(first <= last);
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
  size_t size_;
  T data_[(MaxSize > 0) ? MaxSize : 1];
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_LIMITED_VECTOR_HPP
