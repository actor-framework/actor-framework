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

#include <tuple>

#include "caf/detail/comparable.hpp"
#include "caf/detail/tuple_vals.hpp"
#include "caf/make_copy_on_write.hpp"

namespace caf {

/// A copy-on-write tuple implementation.
template <class... Ts>
class cow_tuple : detail::comparable<cow_tuple<Ts...>>,
                  detail::comparable<cow_tuple<Ts...>, std::tuple<Ts...>> {
public:
  // -- member types -----------------------------------------------------------

  using impl = detail::tuple_vals<Ts...>;

  using data_type = std::tuple<Ts...>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit cow_tuple(Ts... xs)
      : ptr_(make_copy_on_write<impl>(std::move(xs)...)) {
    // nop
  }

  cow_tuple() : ptr_(make_copy_on_write<impl>()) {
    // nop
  }

  cow_tuple(cow_tuple&&) = default;

  cow_tuple(const cow_tuple&) = default;

  cow_tuple& operator=(cow_tuple&&) = default;

  cow_tuple& operator=(const cow_tuple&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns the managed tuple.
  const data_type& data() const noexcept {
    return ptr_->data();
  }

  /// Returns a mutable reference to the managed tuple, guaranteed to have a
  /// reference count of 1.
  data_type& unshared() {
    return ptr_.unshared().data();
  }

  /// Returns whether the reference count of the managed object is 1.
  bool unique() const noexcept {
    return ptr_->unique();
  }

  /// @private
  const intrusive_cow_ptr<impl>& ptr() const noexcept {
    return ptr_;
  }

  // -- comparison -------------------------------------------------------------

  template <class... Us>
  int compare(const std::tuple<Us...>& other) const noexcept {
    return data() < other ? -1 : (data() == other ? 0 : 1);
  }

  template <class... Us>
  int compare(const cow_tuple<Us...>& other) const noexcept {
    return compare(other.data());
  }

private:
  // -- member variables -------------------------------------------------------

  intrusive_cow_ptr<impl> ptr_;
};

/// @relates cow_tuple
template <class Inspector, class... Ts>
typename std::enable_if<Inspector::reads_state,
                        typename Inspector::result_type>::type
inspect(Inspector& f, const cow_tuple<Ts...>& x) {
  return f(x.data());
}

/// @relates cow_tuple
template <class Inspector, class... Ts>
typename std::enable_if<Inspector::writes_state,
                        typename Inspector::result_type>::type
inspect(Inspector& f, cow_tuple<Ts...>& x) {
  return f(x.unshared());
}

/// Creates a new copy-on-write tuple from given arguments.
/// @relates cow_tuple
template <class... Ts>
cow_tuple<typename std::decay<Ts>::type...> make_cow_tuple(Ts&&... xs) {
  return cow_tuple<typename std::decay<Ts>::type...>{std::forward<Ts>(xs)...};
}

/// Convenience function for calling `get<N>(xs.data())`.
/// @relates cow_tuple
template <size_t N, class... Ts>
auto get(const cow_tuple<Ts...>& xs) -> decltype(std::get<N>(xs.data())) {
  return std::get<N>(xs.data());
}

} // namespace caf
