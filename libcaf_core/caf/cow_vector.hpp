// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/intrusive_cow_ptr.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"

#include <initializer_list>
#include <vector>

namespace caf {

/// A copy-on-write vector implementation that wraps a `std::vector`.
template <class T>
class cow_vector {
public:
  // -- member types -----------------------------------------------------------

  using value_type = T;

  using std_type = std::vector<value_type>;

  using size_type = typename std_type::size_type;

  using const_iterator = typename std_type::const_iterator;

  using difference_type = typename std_type::difference_type;

  using const_reverse_iterator = typename std_type::const_reverse_iterator;

  // -- constructors, destructors, and assignment operators --------------------

  cow_vector() {
    impl_ = make_counted<impl>();
  }

  explicit cow_vector(std_type std) {
    impl_ = make_counted<impl>(std::move(std));
  }

  explicit cow_vector(std::initializer_list<T> values) {
    impl_ = make_counted<impl>(std_type{values});
  }

  cow_vector(cow_vector&&) noexcept = default;

  cow_vector(const cow_vector&) noexcept = default;

  cow_vector& operator=(cow_vector&&) noexcept = default;

  cow_vector& operator=(const cow_vector&) noexcept = default;

  // -- properties -------------------------------------------------------------

  /// Returns a mutable reference to the managed vector. Copies the vector if
  /// more than one reference to it exists to make sure the reference count is
  /// exactly 1 when returning from this function.
  std_type& unshared() {
    return impl_.unshared().std;
  }

  /// Returns the managed STD container.
  const std_type& std() const noexcept {
    return impl_->std;
  }

  /// Returns whether the reference count of the managed object is 1.
  [[nodiscard]] bool unique() const noexcept {
    return impl_->unique();
  }

  [[nodiscard]] bool empty() const noexcept {
    return impl_->std.empty();
  }

  size_type size() const noexcept {
    return impl_->std.size();
  }

  size_type max_size() const noexcept {
    return impl_->std.max_size();
  }

  // -- element access ---------------------------------------------------------

  T at(size_type pos) const {
    return impl_->std.at(pos);
  }

  T operator[](size_type pos) const {
    return impl_->std[pos];
  }

  T front() const {
    return impl_->std.front();
  }

  T back() const {
    return impl_->std.back();
  }

  const T* data() const noexcept {
    return impl_->std.data();
  }

  // -- iterator access --------------------------------------------------------

  const_iterator begin() const noexcept {
    return impl_->std.begin();
  }

  const_iterator cbegin() const noexcept {
    return impl_->std.begin();
  }

  const_reverse_iterator rbegin() const noexcept {
    return impl_->std.rbegin();
  }

  const_reverse_iterator crbegin() const noexcept {
    return impl_->std.rbegin();
  }

  const_iterator end() const noexcept {
    return impl_->std.end();
  }

  const_iterator cend() const noexcept {
    return impl_->std.end();
  }

  const_reverse_iterator rend() const noexcept {
    return impl_->std.rend();
  }

  const_reverse_iterator crend() const noexcept {
    return impl_->std.rend();
  }

  // -- friends ----------------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, cow_vector& x) {
    if constexpr (Inspector::is_loading) {
      return f.apply(x.unshared());
    } else {
      return f.apply(x.impl_->std);
    }
  }

private:
  struct impl : ref_counted {
    std_type std;

    impl() = default;

    explicit impl(std_type in) : std(std::move(in)) {
      // nop
    }

    impl* copy() const {
      return new impl{std};
    }
  };

  intrusive_cow_ptr<impl> impl_;
};

// -- comparison ---------------------------------------------------------------

template <class T>
auto operator==(const cow_vector<T>& xs,
                const cow_vector<T>& ys) -> decltype(xs.std() == ys.std()) {
  return xs.std() == ys.std();
}

template <class T>
auto operator==(const cow_vector<T>& xs,
                const std::vector<T>& ys) -> decltype(xs.std() == ys) {
  return xs.std() == ys;
}

template <class T>
auto operator==(const std::vector<T>& xs,
                const cow_vector<T>& ys) -> decltype(xs == ys.std()) {
  return xs.std() == ys;
}

template <class T>
auto operator!=(const cow_vector<T>& xs,
                const cow_vector<T>& ys) -> decltype(xs.std() != ys.std()) {
  return xs.std() != ys.std();
}

template <class T>
auto operator!=(const cow_vector<T>& xs,
                const std::vector<T>& ys) -> decltype(xs.std() != ys) {
  return xs.std() != ys;
}

template <class T>
auto operator!=(const std::vector<T>& xs,
                const cow_vector<T>& ys) -> decltype(xs != ys.std()) {
  return xs.std() != ys;
}

} // namespace caf
