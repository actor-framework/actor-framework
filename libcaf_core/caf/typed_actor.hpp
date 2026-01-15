// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_deprecated.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/to_statically_typed_trait.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/type_id_list.hpp"
#include "caf/typed_actor_view_base.hpp"
#include "caf/typed_behavior.hpp"

#include <concepts>
#include <cstddef>
#include <functional>

namespace caf {

/// Identifies a statically typed actor.
template <class... Ts>
  requires typed_actor_pack<Ts...>
class typed_actor : detail::comparable<typed_actor<Ts...>>,
                    detail::comparable<typed_actor<Ts...>, actor>,
                    detail::comparable<typed_actor<Ts...>, actor_addr>,
                    detail::comparable<typed_actor<Ts...>, strong_actor_ptr> {
public:
  // -- member types -----------------------------------------------------------

  /// Stores the template parameter pack.
  using trait = detail::to_statically_typed_trait_t<Ts...>;

  /// Stores the template parameter pack.
  using signatures = typename trait::signatures;

  // -- friends ----------------------------------------------------------------

  template <class... Us>
    requires typed_actor_pack<Us...>
  friend class typed_actor;

  friend class local_actor;
  friend class abstract_actor;

  template <class>
  friend class data_processor;

  // allow conversion via actor_cast
  template <class, class, int>
  friend class actor_cast_access;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = false;

  /// Creates a new `typed_actor` type by extending this one with `Es...`.
  template <class... Es>
  using extend = detail::extend_with_helper_t<signatures, type_list<Es...>>;

  /// Creates a new `typed_actor` type by extending this one with another
  /// `typed_actor`.
  template <class... Es>
  using extend_with
    = detail::extend_with_helper_t<signatures, typename Es::signatures...>;

  /// Identifies the behavior type actors of this kind use
  /// for their behavior stack.
  using behavior_type = typed_behavior<Ts...>;

  /// The default, event-based type for implementing this messaging interface.
  using impl = typed_event_based_actor<Ts...>;

  /// Identifies pointers to instances of this kind of actor.
  using pointer = impl*;

  /// A view to an actor that implements this messaging interface without
  /// knowledge of the actual type.
  using pointer_view = typed_actor_pointer<Ts...>;

  /// A class type suitable as base type class-based implementations.
  using base = impl;

  /// The default, event-based type for implementing this messaging interface as
  /// a stateful actor.
  template <class State>
  using stateful_impl = stateful_actor<State, impl>;

  /// Convenience alias for `stateful_impl<State>*`.
  template <class State>
  using stateful_pointer = stateful_impl<State>*;

  /// Identifies the base class of brokers implementing this interface.
  using broker_base = typename detail::broker_from_signatures<signatures>::type;

  /// Identifies pointers to brokers implementing this interface.
  using broker_pointer = broker_base*;

  /// Identifies the broker_base class for this kind of actor with actor.
  template <class State>
  using stateful_broker_base = stateful_actor<State, broker_base>;

  /// Identifies the broker_base class for this kind of actor with actor.
  template <class State>
  using stateful_broker_pointer = stateful_actor<State, broker_base>*;

  typed_actor() = default;
  typed_actor(typed_actor&&) = default;
  typed_actor(const typed_actor&) = default;
  typed_actor& operator=(typed_actor&&) = default;
  typed_actor& operator=(const typed_actor&) = default;

  template <class... Us>
    requires detail::tl_subset_of_v<signatures,
                                    typename typed_actor<Us...>::signatures>
  typed_actor(const typed_actor<Us...>& other) : ptr_(other.ptr()) {
    // nop
  }

  // allow `handle_type{this}` for typed actors
  template <class T>
    requires(actor_traits<T>::is_statically_typed
             && detail::tl_subset_of_v<signatures, typename T::signatures>)
  typed_actor(T* ptr) : ptr_(ptr->ctrl(), add_ref) {
    CAF_ASSERT(ptr != nullptr);
  }

  // Enable `handle_type{self}` for typed actor views.
  template <std::derived_from<typed_actor_view_base> T>
    requires detail::tl_subset_of_v<signatures, typename T::signatures>
  explicit typed_actor(T ptr) : ptr_(ptr.ctrl(), add_ref) {
    // nop
  }

  template <class... Us>
    requires detail::tl_subset_of_v<signatures,
                                    typename typed_actor<Us...>::signatures>
  typed_actor& operator=(const typed_actor<Us...>& other) {
    ptr_ = other.ptr_;
    return *this;
  }

  typed_actor& operator=(std::nullptr_t) {
    ptr_.reset();
    return *this;
  }

  /// Queries whether this actor handle is valid.
  explicit operator bool() const {
    return static_cast<bool>(ptr_);
  }

  /// Queries whether this actor handle is invalid.
  bool operator!() const {
    return !ptr_;
  }

  /// Queries the address of the stored actor.
  actor_addr address() const noexcept {
    return {ptr_.get(), add_ref};
  }

  /// Returns the ID of this actor.
  actor_id id() const noexcept {
    return ptr_->id();
  }

  /// Returns the origin node of this actor.
  node_id node() const noexcept {
    return ptr_->node();
  }

  /// Returns the hosting actor system.
  actor_system& home_system() const noexcept {
    return *ptr_->home_system;
  }

  /// Exchange content of `*this` and `other`.
  void swap(typed_actor& other) noexcept {
    ptr_.swap(other.ptr_);
  }

  /// @cond

  abstract_actor* operator->() const noexcept {
    return ptr_->get();
  }

  abstract_actor& operator*() const noexcept {
    return *ptr_->get();
  }

  const strong_actor_ptr& ptr() const noexcept {
    return ptr_;
  }

  intptr_t compare(const typed_actor& x) const noexcept {
    return actor_addr::compare(get(), x.get());
  }

  intptr_t compare(const actor& x) const noexcept {
    return actor_addr::compare(get(), actor_cast<actor_control_block*>(x));
  }

  intptr_t compare(const actor_addr& x) const noexcept {
    return actor_addr::compare(get(), actor_cast<actor_control_block*>(x));
  }

  intptr_t compare(const strong_actor_ptr& x) const noexcept {
    return actor_addr::compare(get(), actor_cast<actor_control_block*>(x));
  }

  CAF_DEPRECATED("construct using add_ref or adopt_ref instead")
  typed_actor(actor_control_block* ptr, bool increase_ref_count) {
    if (increase_ref_count) {
      ptr_.reset(ptr, add_ref);
    } else {
      ptr_.reset(ptr, adopt_ref);
    }
  }

  typed_actor(actor_control_block* ptr, add_ref_t) : ptr_(ptr, add_ref) {
    // nop
  }

  typed_actor(actor_control_block* ptr, adopt_ref_t) : ptr_(ptr, adopt_ref) {
    // nop
  }

  friend std::string to_string(const typed_actor& x) {
    return to_string(x.ptr_);
  }

  friend void append_to_string(std::string& x, const typed_actor& y) {
    return append_to_string(x, y.ptr_);
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, typed_actor& x) {
    return f.value(x.ptr_);
  }

  /// Releases the reference held by handle `x`. Using the
  /// handle after invalidating it is undefined behavior.
  friend void destroy(typed_actor& x) {
    x.ptr_.reset();
  }

  static auto allowed_inputs() {
    return detail::make_signatures_type_id_list(signatures{});
  }

  /// @endcond

private:
  actor_control_block* get() const noexcept {
    return ptr_.get();
  }

  actor_control_block* release() noexcept {
    return ptr_.release();
  }

  CAF_DEPRECATED("construct using add_ref or adopt_ref instead")
  explicit typed_actor(actor_control_block* ptr) : ptr_(ptr, add_ref) {
    // nop
  }

  strong_actor_ptr ptr_;
};

/// @relates typed_actor
template <class... Xs, class... Ys>
bool operator==(const typed_actor<Xs...>& x,
                const typed_actor<Ys...>& y) noexcept {
  return actor_addr::compare(actor_cast<actor_control_block*>(x),
                             actor_cast<actor_control_block*>(y))
         == 0;
}

/// @relates typed_actor
template <class... Xs, class... Ys>
bool operator!=(const typed_actor<Xs...>& x,
                const typed_actor<Ys...>& y) noexcept {
  return !(x == y);
}

/// @relates typed_actor
template <class... Xs>
bool operator==(const typed_actor<Xs...>& x, std::nullptr_t) noexcept {
  return actor_addr::compare(actor_cast<actor_control_block*>(x), nullptr) == 0;
}

/// @relates typed_actor
template <class... Xs>
bool operator==(std::nullptr_t, const typed_actor<Xs...>& x) noexcept {
  return actor_addr::compare(actor_cast<actor_control_block*>(x), nullptr) == 0;
}

/// @relates typed_actor
template <class... Xs>
bool operator!=(const typed_actor<Xs...>& x, std::nullptr_t) noexcept {
  return !(x == nullptr);
}

/// @relates typed_actor
template <class... Xs>
bool operator!=(std::nullptr_t, const typed_actor<Xs...>& x) noexcept {
  return !(x == nullptr);
}

} // namespace caf

// allow typed_actor to be used in hash maps
namespace std {
template <class... Sigs>
struct hash<caf::typed_actor<Sigs...>> {
  size_t operator()(const caf::typed_actor<Sigs...>& ref) const {
    return ref ? static_cast<size_t>(ref->id()) : 0;
  }
};
} // namespace std
