// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>
#include <cstring>
#include <iosfwd>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>

namespace caf {

namespace detail {

// Catches `std::string`, `std::string_view` and all classes mimicking those,
// but not `std::vector<char>` or other buffers.
template <class T>
struct is_string_like {
  // SFINAE checks.
  template <class U>
  static bool sfinae(
    const U* x,
    // check if `x->data()` returns  const char*
    std::enable_if_t<
      std::is_same<const char*, decltype(x->data())>::value>* = nullptr,
    // check if `x->size()` returns an integer
    std::enable_if_t<std::is_integral<decltype(x->size())>::value>* = nullptr,
    // check if `x->find('?', 0)` is well-formed and returns an integer
    // (distinguishes vectors from strings)
    std::enable_if_t<
      std::is_integral<decltype(x->find('?', 0))>::value>* = nullptr);

  // SFINAE fallback.
  static void sfinae(void*);

  // Result of SFINAE test.
  using result_type
    = decltype(sfinae(static_cast<typename std::decay<T>::type*>(nullptr)));

  // Trait result.
  static constexpr bool value = std::is_same_v<bool, result_type>;
};

} // namespace detail

/// Drop-in replacement for C++17 std::string_view.
class CAF_CORE_EXPORT string_view : detail::comparable<string_view> {
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
    : data_(cstr), size_(length) {
    // nop
  }

  constexpr string_view(iterator first, iterator last) noexcept
    : data_(first), size_(static_cast<size_t>(last - first)) {
    // nop
  }

#ifdef CAF_GCC
  constexpr string_view(const char* cstr) noexcept
    : data_(cstr), size_(strlen(cstr)) {
    // nop
  }
#else
  template <size_t N>
  constexpr string_view(const char (&cstr)[N]) noexcept
    : data_(cstr), size_(N - 1) {
    // nop
  }
#endif

  constexpr string_view(const string_view&) noexcept = default;

  template <class T, class = typename std::enable_if<
                       detail::is_string_like<T>::value>::type>
  constexpr string_view(const T& str) noexcept
    : data_(str.data()), size_(str.size()) {
    // nop
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

  // -- algorithms -------------------------------------------------------------

  size_type copy(pointer dest, size_type n, size_type pos = 0) const;

  string_view substr(size_type pos = 0, size_type n = npos) const noexcept;

  int compare(string_view s) const noexcept;

  int compare(size_type pos1, size_type n1, string_view str) const noexcept;

  int compare(size_type pos1, size_type n1, string_view str, size_type pos2,
              size_type n2) const noexcept;

  int compare(const_pointer str) const noexcept;

  int compare(size_type pos, size_type n, const_pointer str) const noexcept;

  int compare(size_type pos1, size_type n1, const_pointer s,
              size_type n2) const noexcept;

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

CAF_PUSH_DEPRECATED_WARNING

/// @relates string_view
inline std::string to_string(string_view x) {
  return std::string{x.begin(), x.end()};
}

} // namespace caf

namespace caf::literals {

constexpr string_view operator""_sv(const char* cstr, size_t len) {
  return {cstr, len};
}

} // namespace caf::literals

namespace std {

CAF_CORE_EXPORT std::ostream& operator<<(std::ostream& out, caf::string_view);

} // namespace std

CAF_POP_WARNINGS
