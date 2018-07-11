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

#include <cstddef>
#include <cstring>
#include <iosfwd>
#include <iterator>
#include <limits>
#include <type_traits>

#include "caf/detail/comparable.hpp"

namespace caf {

namespace detail {

// Catches `std::string`, `std::string_view` and all classes mimicing those,
// but not `std::vector<char>` or other buffers.
template <class T>
struct is_string_like {
  // SFINAE checks.
  template <class U>
  static bool sfinae(
    const U* x,
    // check if `(*x)[0]` returns `const char&`
    typename std::enable_if<
      std::is_same<
        const char&,
        decltype((*x)[0])
      >::value
    >::type* = nullptr,
    // check if `x->size()` returns an integer
    typename std::enable_if<
      std::is_integral<
        decltype(x->size())
      >::value
    >::type* = nullptr,
    // check if `x->find('?', 0)` is well-formed and returns an integer
    // (distinguishes vectors from strings)
    typename std::enable_if<
      std::is_integral<
        decltype(x->find('?', 0))
      >::value
    >::type* = nullptr);

  // SFINAE fallback.
  static void sfinae(void*);

  // Result of SFINAE test.
  using result_type =
    decltype(sfinae(static_cast<typename std::decay<T>::type*>(nullptr)));

  // Trait result.
  static constexpr bool value = std::is_same<bool, result_type>::value;
};

} // namespace detail

/// Drop-in replacement for C++17 std::string_view.
class string_view : detail::comparable<string_view> {
public:
  // -- member types -----------------------------------------------------------

  using value_type = char;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;
  using const_iterator = const_pointer;
  using iterator = const_iterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = const_reverse_iterator;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  // -- constants --------------------------------------------------------------

  static constexpr size_type npos = std::numeric_limits<size_type>::max();

  // -- constructors, destructors, and assignment operators --------------------

  constexpr string_view() noexcept : data_(nullptr), size_(0) {
    // nop
  }

  constexpr string_view(const char* cstr, size_t length) noexcept
      : data_(cstr),
        size_(length) {
    // nop
  }

#ifdef CAF_GCC
  constexpr string_view(const char* cstr) noexcept
      : data_(cstr),
        size_(strlen(cstr)) {
    // nop
  }
#else
  template <size_t N>
  constexpr string_view(const char (&cstr)[N]) noexcept
      : data_(cstr),
        size_(N - 1) {
    // nop
  }
#endif

  constexpr string_view(const string_view&) noexcept = default;

  template <
    class T,
    class = typename std::enable_if<detail::is_string_like<T>::value>::type
  >
  string_view(const T& str) noexcept {
    auto len = str.size();
    if (len == 0) {
      data_ = nullptr;
      size_ = 0;
    } else {
      data_ = &(str[0]);
      size_ = str.size();
    }
  }

  string_view& operator=(const string_view&) noexcept = default;

  // -- capacity ---------------------------------------------------------------

  constexpr size_type size() const noexcept {
    return size_;
  }

  constexpr size_type length() const noexcept {
    return size_;
  }

  constexpr size_type max_size() const noexcept {
    return std::numeric_limits<size_type>::max();
  }

  constexpr bool empty() const noexcept {
    return size_ == 0;
  }

  // -- iterator access --------------------------------------------------------

  constexpr const_iterator begin() const noexcept {
    return data_;
  }

  constexpr const_iterator end() const noexcept {
    return data_ + size_;
  }

  constexpr const_iterator cbegin() const noexcept {
    return begin();
  }

  constexpr const_iterator cend() const noexcept {
    return end();
  }

  const_reverse_iterator rbegin() const noexcept;

  const_reverse_iterator rend() const noexcept;

  const_reverse_iterator crbegin() const noexcept;

  const_reverse_iterator crend() const noexcept;

  // -- element access ---------------------------------------------------------

  constexpr const_reference operator[](size_type pos) const {
    return data_[pos];
  }

  const_reference at(size_type pos) const;

  constexpr const_reference front() const {
    return *data_;
  }

  constexpr const_reference back() const {
    return data_[size_ - 1];
  }

  constexpr const_pointer data() const noexcept {
    return data_;
  }

  // -- modifiers --------------------------------------------------------------

  void remove_prefix(size_type n);

  void remove_suffix(size_type n);

  void assign(const_pointer data, size_type len);

  // -- algortihms -------------------------------------------------------------

  size_type copy(pointer dest, size_type n, size_type pos = 0) const;

  string_view substr(size_type pos = 0, size_type n = npos) const noexcept;

  int compare(string_view s) const noexcept;

  int compare(size_type pos1, size_type n1, string_view str) const noexcept;

  int compare(size_type pos1, size_type n1, string_view str,
              size_type pos2, size_type n2) const noexcept;

  int compare(const_pointer str) const noexcept;

  int compare(size_type pos, size_type n, const_pointer str) const noexcept;

  int compare(size_type pos1, size_type n1,
              const_pointer s, size_type n2) const noexcept;

  size_type find(string_view str, size_type pos = 0) const noexcept;

  size_type find(value_type ch, size_type pos = 0) const noexcept;

  size_type find(const_pointer str, size_type pos, size_type n) const noexcept;

  size_type find(const_pointer str, size_type pos = 0) const noexcept;

  size_type rfind(string_view str, size_type pos = npos) const noexcept;

  size_type rfind(value_type ch, size_type pos = npos) const noexcept;

  size_type rfind(const_pointer str, size_type pos, size_type n) const noexcept;

  size_type rfind(const_pointer str, size_type pos = npos) const noexcept;

  size_type find_first_of(string_view str, size_type pos = 0) const noexcept;

  size_type find_first_of(value_type ch, size_type pos = 0) const noexcept;

  size_type find_first_of(const_pointer str, size_type pos,
                          size_type n) const noexcept;

  size_type find_first_of(const_pointer str, size_type pos = 0) const noexcept;

  size_type find_last_of(string_view str, size_type pos = npos) const noexcept;

  size_type find_last_of(value_type ch, size_type pos = npos) const noexcept;

  size_type find_last_of(const_pointer str, size_type pos,
                         size_type n) const noexcept;

  size_type find_last_of(const_pointer str,
                         size_type pos = npos) const noexcept;

  size_type find_first_not_of(string_view str,
                              size_type pos = 0) const noexcept;

  size_type find_first_not_of(value_type ch, size_type pos = 0) const noexcept;

  size_type find_first_not_of(const_pointer str, size_type pos,
                              size_type n) const noexcept;

  size_type find_first_not_of(const_pointer str,
                              size_type pos = 0) const noexcept;

  size_type find_last_not_of(string_view str,
                             size_type pos = npos) const noexcept;

  size_type find_last_not_of(value_type ch,
                             size_type pos = npos) const noexcept;

  size_type find_last_not_of(const_pointer str, size_type pos,
                             size_type n) const noexcept;

  size_type find_last_not_of(const_pointer str,
                             size_type pos = npos) const noexcept;

private:
  const char* data_;
  size_t size_;
};

} // namespace caf

namespace std {

std::ostream& operator<<(std::ostream& out, caf::string_view);

} // namespace std
