// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"

#include <array>
#include <cstddef>
#include <type_traits>

namespace caf {

/// A C++11/14 drop-in replacement for C++20's `std::span` without support for
/// static extents.
template <class T>
class span {
public:
  // -- member types -----------------------------------------------------------

  using element_type = T;

  using value_type = std::remove_cv_t<T>;

  using index_type = size_t;

  using difference_type = ptrdiff_t;

  using pointer = T*;

  using const_pointer = const T*;

  using reference = T&;

  using const_reference = T&;

  using iterator = pointer;

  using const_iterator = const_pointer;

  using reverse_iterator = std::reverse_iterator<iterator>;

  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr span() noexcept : begin_(nullptr), size_(0) {
    // nop
  }

  constexpr span(pointer ptr, size_t size) : begin_(ptr), size_(size) {
    // nop
  }

  constexpr span(pointer first, pointer last)
    : begin_(first), size_(static_cast<size_t>(last - first)) {
    // nop
  }

  template <size_t Size>
  constexpr span(element_type (&arr)[Size]) noexcept
    : begin_(arr), size_(Size) {
    // nop
  }

  template <class C, class = std::enable_if_t<
                       detail::has_convertible_data_member_v<C, value_type>>>
  span(C& xs) noexcept : begin_(xs.data()), size_(xs.size()) {
    // nop
  }

  template <class C, class = std::enable_if_t<
                       detail::has_convertible_data_member_v<C, value_type>>>
  span(const C& xs) noexcept : begin_(xs.data()), size_(xs.size()) {
    // nop
  }

  constexpr span(const span&) noexcept = default;

  span& operator=(const span&) noexcept = default;

  // -- iterators --------------------------------------------------------------

  constexpr iterator begin() const noexcept {
    return begin_;
  }

  constexpr const_iterator cbegin() const noexcept {
    return begin_;
  }

  constexpr iterator end() const noexcept {
    return begin() + size_;
  }

  constexpr const_iterator cend() const noexcept {
    return cbegin() + size_;
  }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator{end()};
  }

  constexpr const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator{end()};
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator{begin()};
  }

  constexpr const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator{begin()};
  }

  // -- element access ---------------------------------------------------------

  constexpr reference operator[](size_t index) const noexcept {
    return begin_[index];
  }

  constexpr reference front() const noexcept {
    return *begin_;
  }

  constexpr reference back() const noexcept {
    return (*this)[size_ - 1];
  }

  // -- properties -------------------------------------------------------------

  constexpr size_t size() const noexcept {
    return size_;
  }

  constexpr size_t size_bytes() const noexcept {
    return size_ * sizeof(element_type);
  }

  constexpr bool empty() const noexcept {
    return size_ == 0;
  }

  constexpr pointer data() const noexcept {
    return begin_;
  }

  // -- subviews ---------------------------------------------------------------

  constexpr span subspan(size_t offset, size_t num_bytes) const {
    return {begin_ + offset, num_bytes};
  }

  constexpr span subspan(size_t offset) const {
    return {begin_ + offset, size_ - offset};
  }

  constexpr span first(size_t num_bytes) const {
    return {begin_, num_bytes};
  }

  constexpr span last(size_t num_bytes) const {
    return subspan(size_ - num_bytes, num_bytes);
  }

private:
  // -- member variables -------------------------------------------------------

  /// Points to the first element in the contiguous memory block.
  pointer begin_;

  /// Stores the number of elements in the contiguous memory block.
  size_t size_;
};

template <class T>
auto begin(const span<T>& xs) -> decltype(xs.begin()) {
  return xs.begin();
}

template <class T>
auto cbegin(const span<T>& xs) -> decltype(xs.cbegin()) {
  return xs.cbegin();
}

template <class T>
auto end(const span<T>& xs) -> decltype(xs.end()) {
  return xs.end();
}

template <class T>
auto cend(const span<T>& xs) -> decltype(xs.cend()) {
  return xs.cend();
}

template <class T>
span<const std::byte> as_bytes(span<T> xs) {
  return {reinterpret_cast<const std::byte*>(xs.data()), xs.size_bytes()};
}

template <class T>
span<std::byte> as_writable_bytes(span<T> xs) {
  return {reinterpret_cast<std::byte*>(xs.data()), xs.size_bytes()};
}

/// Convenience function to make using `caf::span` more convenient without the
/// deduction guides.
template <class T>
auto make_span(T& xs) -> span<std::remove_reference_t<decltype(xs[0])>> {
  return {xs.data(), xs.size()};
}

/// Convenience function to make using `caf::span` more convenient without the
/// deduction guides.
template <class T, size_t N>
span<T> make_span(T (&xs)[N]) {
  return {xs, N};
}

/// Convenience function to make using `caf::span` more convenient without the
/// deduction guides.
template <class T>
span<T> make_span(T* first, size_t size) {
  return {first, size};
}

/// Convenience function to make using `caf::span` more convenient without the
/// deduction guides.
template <class T>
span<T> make_span(T* first, T* last) {
  return {first, last};
}

} // namespace caf
