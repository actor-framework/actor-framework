// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/typed_actor_view.hpp"

namespace caf {

/// Provides a view to an actor that implements this messaging interface without
/// knowledge of the actual type.
template <class... Sigs>
class typed_actor_pointer : public typed_actor_view_base {
public:
  /// Stores the template parameter pack.
  using signatures = detail::type_list<Sigs...>;

  typed_actor_pointer() : view_(nullptr) {
    // nop
  }

  template <class Supertype,
            class = detail::enable_if_t< //
              detail::tl_subset_of<detail::type_list<Sigs...>,
                                   typename Supertype::signatures>::value>>
  typed_actor_pointer(Supertype* selfptr) : view_(selfptr) {
    // nop
  }

  template <class... OtherSigs,
            class = detail::enable_if_t< //
              detail::tl_subset_of<detail::type_list<Sigs...>,
                                   detail::type_list<OtherSigs...>>::value>>
  typed_actor_pointer(typed_actor_pointer<OtherSigs...> other)
    : view_(other.internal_ptr()) {
    // nop
  }

  typed_actor_pointer(const typed_actor_pointer&) = default;

  explicit typed_actor_pointer(std::nullptr_t) : view_(nullptr) {
    // nop
  }

  typed_actor_pointer& operator=(const typed_actor_pointer&) = default;

  template <class Supertype>
  typed_actor_pointer& operator=(Supertype* ptr) {
    using namespace detail;
    static_assert(
      tl_subset_of<type_list<Sigs...>, typename Supertype::signatures>::value,
      "cannot assign pointer of unrelated actor type");
    view_.reset(ptr);
    return *this;
  }

  template <class... OtherSigs,
            class = detail::enable_if_t< //
              detail::tl_subset_of<detail::type_list<Sigs...>,
                                   detail::type_list<OtherSigs...>>::value>>
  typed_actor_pointer& operator=(typed_actor_pointer<OtherSigs...> other) {
    using namespace detail;
    static_assert(
      tl_subset_of<type_list<Sigs...>, type_list<OtherSigs...>>::value,
      "cannot assign pointer of unrelated actor type");
    view_.reset(other.internal_ptr());
    return *this;
  }

  typed_actor_view<Sigs...>* operator->() {
    return &view_;
  }

  const typed_actor_view<Sigs...>* operator->() const {
    return &view_;
  }

  bool operator!() const noexcept {
    return internal_ptr() == nullptr;
  }

  explicit operator bool() const noexcept {
    return internal_ptr() != nullptr;
  }

  /// @private
  actor_control_block* get() const {
    return view_.ctrl();
  }

  /// @private
  actor_control_block* ctrl() const noexcept {
    return view_.ctrl();
  }

  /// @private
  scheduled_actor* internal_ptr() const noexcept {
    return view_.internal_ptr();
  }

  operator scheduled_actor*() const noexcept {
    return view_.internal_ptr();
  }

private:
  typed_actor_view<Sigs...> view_;
};

} // namespace caf
