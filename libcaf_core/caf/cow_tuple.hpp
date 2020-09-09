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
#include "caf/inspector_access.hpp"
#include "caf/make_copy_on_write.hpp"
#include "caf/ref_counted.hpp"

namespace caf {

/// A copy-on-write tuple implementation.
template <class... Ts>
class cow_tuple : detail::comparable<cow_tuple<Ts...>>,
                  detail::comparable<cow_tuple<Ts...>, std::tuple<Ts...>> {
public:
  // -- member types -----------------------------------------------------------

  using data_type = std::tuple<Ts...>;

  struct impl : ref_counted {
    template <class... Us>
    impl(Us&&... xs) : data(std::forward<Us>(xs)...) {
      // nop
    }

    impl() = default;

    impl(const impl&) = default;

    impl& operator=(const impl&) = default;

    impl* copy() const {
      return new impl{*this};
    }

    data_type data;
  };

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
    return ptr_->data;
  }

  /// Returns a mutable reference to the managed tuple, guaranteed to have a
  /// reference count of 1.
  data_type& unshared() {
    return ptr_.unshared().data;
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

// -- inspection ---------------------------------------------------------------

template <class... Ts>
struct inspector_access<cow_tuple<Ts...>> {
  using value_type = cow_tuple<Ts...>;

  template <class Inspector>
  static bool apply_object(Inspector& f, value_type& x) {
    if constexpr (Inspector::is_loading)
      return detail::load_object(f, x.unshared());
    else
      return detail::save_object(f, detail::as_mutable_ref(x.data()));
  }

  template <class Inspector>
  static bool apply_value(Inspector& f, value_type& x) {
    if constexpr (Inspector::is_loading)
      return detail::load_value(f, x.unshared());
    else
      return detail::save_value(f, detail::as_mutable_ref(x.data()));
  }

  template <class Inspector>
  static bool save_field(Inspector& f, string_view field_name, value_type& x) {
    return detail::save_field(f, field_name, detail::as_mutable_ref(x.data()));
  }

  template <class Inspector, class IsPresent, class Get>
  static bool save_field(Inspector& f, string_view field_name,
                         IsPresent& is_present, Get& get) {
    if constexpr (std::is_lvalue_reference<decltype(get())>::value) {
      auto get_data = [&get]() -> decltype(auto) {
        return detail::as_mutable_ref(get().data());
      };
      return detail::save_field(f, field_name, is_present, get_data);
    } else {
      auto get_data = [&get] {
        auto tmp = get();
        return std::move(tmp.unshared());
      };
      return detail::save_field(f, field_name, is_present, get_data);
    }
  }

  template <class Inspector, class IsValid, class SyncValue>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value) {
    return detail::load_field(f, field_name, x.unshared(), is_valid,
                              sync_value);
  }

  template <class Inspector, class IsValid, class SyncValue, class SetFallback>
  static bool load_field(Inspector& f, string_view field_name, value_type& x,
                         IsValid& is_valid, SyncValue& sync_value,
                         SetFallback& set_fallback) {
    return detail::load_field(f, field_name, x.unshared(), is_valid, sync_value,
                              set_fallback);
  }
};

} // namespace caf
