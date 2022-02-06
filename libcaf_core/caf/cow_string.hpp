// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/intrusive_cow_ptr.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/string_algorithms.hpp"

#include <string>
#include <string_view>

namespace caf {

/// A copy-on-write string implementation that wraps a `std::basic_string`.
template <class CharT>
class basic_cow_string
  : detail::comparable<basic_cow_string<CharT>>,
    detail::comparable<basic_cow_string<CharT>, std::basic_string<CharT>>,
    detail::comparable<basic_cow_string<CharT>, const CharT*> {
public:
  // -- member types -----------------------------------------------------------

  using std_type = std::basic_string<CharT>;

  using view_type = std::basic_string_view<CharT>;

  using size_type = typename std_type::size_type;

  using const_iterator = typename std_type::const_iterator;

  using const_reverse_iterator = typename std_type::const_reverse_iterator;

  // -- constants --------------------------------------------------------------

  static inline const size_type npos = std_type::npos;

  // -- constructors, destructors, and assignment operators --------------------

  basic_cow_string() {
    impl_ = make_counted<impl>();
  }

  explicit basic_cow_string(std_type str) {
    impl_ = make_counted<impl>(std::move(str));
  }

  explicit basic_cow_string(view_type str) {
    impl_ = make_counted<impl>(std_type{str});
  }

  basic_cow_string(basic_cow_string&&) noexcept = default;

  basic_cow_string(const basic_cow_string&) noexcept = default;

  basic_cow_string& operator=(basic_cow_string&&) noexcept = default;

  basic_cow_string& operator=(const basic_cow_string&) noexcept = default;

  // -- properties -------------------------------------------------------------

  /// Returns a mutable reference to the managed string. Copies the string if
  /// more than one reference to it exists to make sure the reference count is
  /// exactly 1 when returning from this function.
  std_type& unshared() {
    return impl_.unshared().str;
  }

  /// Returns the managed string.
  const std_type& str() const noexcept {
    return impl_->str;
  }

  /// Returns whether the reference count of the managed object is 1.
  [[nodiscard]] bool unique() const noexcept {
    return impl_->unique();
  }

  [[nodiscard]] bool empty() const noexcept {
    return impl_->str.empty();
  }

  size_type size() const noexcept {
    return impl_->str.size();
  }

  size_type length() const noexcept {
    return impl_->str.length();
  }

  size_type max_size() const noexcept {
    return impl_->str.max_size();
  }

  // -- element access ---------------------------------------------------------

  CharT at(size_type pos) const {
    return impl_->str.at(pos);
  }

  CharT operator[](size_type pos) const {
    return impl_->str[pos];
  }

  CharT front() const {
    return impl_->str.front();
  }

  CharT back() const {
    return impl_->str.back();
  }

  const CharT* data() const noexcept {
    return impl_->str.data();
  }

  const CharT* c_str() const noexcept {
    return impl_->str.c_str();
  }

  // -- conversion and copying -------------------------------------------------

  operator view_type() const noexcept {
    return view_type{impl_->str};
  }

  basic_cow_string substr(size_type pos = 0, size_type count = npos) const {
    return basic_cow_string{impl_->str.substr(pos, count)};
  }

  size_type copy(CharT* dest, size_type count, size_type pos = 0) const {
    return impl_->str.copy(dest, count, pos);
  }

  // -- iterator access --------------------------------------------------------

  const_iterator begin() const noexcept {
    return impl_->str.begin();
  }

  const_iterator cbegin() const noexcept {
    return impl_->str.begin();
  }

  const_reverse_iterator rbegin() const noexcept {
    return impl_->str.rbegin();
  }

  const_reverse_iterator crbegin() const noexcept {
    return impl_->str.rbegin();
  }

  const_iterator end() const noexcept {
    return impl_->str.end();
  }

  const_iterator cend() const noexcept {
    return impl_->str.end();
  }

  const_reverse_iterator rend() const noexcept {
    return impl_->str.rend();
  }

  const_reverse_iterator crend() const noexcept {
    return impl_->str.rend();
  }

  // -- predicates -------------------------------------------------------------

  bool starts_with(view_type x) const noexcept {
    return caf::starts_with(impl_->str, x);
  }

  bool starts_with(CharT x) const noexcept {
    return empty() ? false : front() == x;
  }

  bool starts_with(const CharT* x) const {
    return starts_with(view_type{x});
  }

  bool ends_with(view_type x) const noexcept {
    return caf::ends_with(impl_->str, x);
  }

  bool ends_with(CharT x) const noexcept {
    return empty() ? false : back() == x;
  }

  bool ends_with(const CharT* x) const {
    return ends_with(view_type{x});
  }

  bool contains(std::string_view x) const noexcept {
    return find(x) != npos;
  }

  bool contains(char x) const noexcept {
    return find(x) != npos;
  }

  bool contains(const CharT* x) const {
    return contains(view_type{x});
  }

  // -- search -----------------------------------------------------------------

  size_type find(const std::string& str, size_type pos = 0) const noexcept {
    return impl_->str.find(str, pos);
  }

  size_type find(const basic_cow_string& str,
                 size_type pos = 0) const noexcept {
    return find(str.impl_->str, pos);
  }

  size_type find(const CharT* str, size_type pos, size_type count) const {
    return impl_->str.find(str, pos, count);
  }

  size_type find(const CharT* str, size_type pos = 0) const {
    return impl_->str.find(str, pos);
  }

  size_type find(char x, size_type pos = 0) const noexcept {
    return impl_->str.find(x, pos);
  }

  template <class T>
  std::enable_if_t<std::is_convertible_v<const T&, view_type> //
                     && !std::is_convertible_v<const T&, const CharT*>,
                   size_type>
  find(const T& x, size_type pos = 0) const noexcept {
    return impl_->str.find(x, pos);
  }

  // -- comparison -------------------------------------------------------------

  int compare(const CharT* x) const noexcept {
    return str().compare(x);
  }

  int compare(const std_type& x) const noexcept {
    return str().compare(x);
  }

  int compare(const cow_string& x) const noexcept {
    return impl_ == x.impl_ ? 0 : compare(x.str());
  }

  // -- friends ----------------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, basic_cow_string& x) {
    if constexpr (Inspector::is_loading) {
      return f.apply(x.unshared());
    } else {
      return f.apply(x.impl_->str);
    }
  }

private:
  struct impl : ref_counted {
    std_type str;

    impl() = default;

    explicit impl(std_type in) : str(std::move(in)) {
      // nop
    }

    impl* copy() const {
      return new impl{str};
    }
  };

  intrusive_cow_ptr<impl> impl_;
};

/// A copy-on-write wrapper for a `std::string`;
/// @relates basic_cow_string
using cow_string = basic_cow_string<char>;

/// A copy-on-write wrapper for a `std::string`;
/// @relates basic_cow_string
using cow_u16string = basic_cow_string<char16_t>;

/// A copy-on-write wrapper for a `std::string`;
/// @relates basic_cow_string
using cow_u32string = basic_cow_string<char32_t>;

} // namespace caf
