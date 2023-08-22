// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/composed_type.hpp"
#include "caf/decorator/sequencer.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_actor.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/type_id_list.hpp"
#include "caf/typed_actor_view_base.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/typed_response_promise.hpp"

#include <cstddef>

namespace caf {

/// Identifies a statically typed actor.
/// @tparam Sigs Function signatures for all accepted messages.
template <class... Sigs>
class typed_actor : detail::comparable<typed_actor<Sigs...>>,
                    detail::comparable<typed_actor<Sigs...>, actor>,
                    detail::comparable<typed_actor<Sigs...>, actor_addr>,
                    detail::comparable<typed_actor<Sigs...>, strong_actor_ptr> {
public:
  static_assert(sizeof...(Sigs) > 0, "Empty typed actor handle");

  static_assert((detail::is_normalized_signature_v<Sigs> && ...),
                "Function signatures must be normalized to the format "
                "'result<Out...>(In...)', no qualifiers or references allowed");

  // -- friend types that need access to private ctors
  friend class local_actor;

  template <class>
  friend class data_processor;

  template <class...>
  friend class typed_actor;

  // allow conversion via actor_cast
  template <class, class, int>
  friend class actor_cast_access;

  // tell actor_cast which semantic this type uses
  static constexpr bool has_weak_ptr_semantics = false;

  /// Stores the template parameter pack.
  using signatures = detail::type_list<Sigs...>;

  /// Creates a new `typed_actor` type by extending this one with `Es...`.
  template <class... Es>
  using extend = typed_actor<Sigs..., Es...>;

  /// Creates a new `typed_actor` type by extending this one with another
  /// `typed_actor`.
  template <class... Ts>
  using extend_with =
    typename detail::extend_with_helper<typed_actor, Ts...>::type;

  /// Identifies the behavior type actors of this kind use
  /// for their behavior stack.
  using behavior_type = typed_behavior<Sigs...>;

  /// The default, event-based type for implementing this messaging interface.
  using impl = typed_event_based_actor<Sigs...>;

  /// Identifies pointers to instances of this kind of actor.
  using pointer = impl*;

  /// A view to an actor that implements this messaging interface without
  /// knowledge of the actual type.
  using pointer_view = typed_actor_pointer<Sigs...>;

  /// A class type suitable as base type class-based implementations.
  using base = impl;

  /// The default, event-based type for implementing this messaging interface as
  /// a stateful actor.
  template <class State>
  using stateful_impl = stateful_actor<State, impl>;

  /// Convenience alias for `stateful_impl<State>*`.
  template <class State>
  using stateful_pointer = stateful_impl<State>*;

  /// Identifies pointers to brokers implementing this interface.
  using broker_pointer = io::typed_broker<Sigs...>*;

  /// Identifies the base class of brokers implementing this interface.
  using broker_base = io::typed_broker<Sigs...>;

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

  template <class... Ts>
  typed_actor(const typed_actor<Ts...>& other) : ptr_(other.ptr_) {
    static_assert(
      detail::tl_subset_of<signatures, detail::type_list<Ts...>>::value,
      "Cannot assign incompatible handle");
  }

  // allow `handle_type{this}` for typed actors
  template <class T,
            class = std::enable_if_t<actor_traits<T>::is_statically_typed>>
  typed_actor(T* ptr) : ptr_(ptr->ctrl()) {
    static_assert(
      detail::tl_subset_of<signatures, typename T::signatures>::value,
      "Cannot assign T* to incompatible handle type");
    CAF_ASSERT(ptr != nullptr);
  }

  // Enable `handle_type{self}` for typed actor views.
  template <class T, class = std::enable_if_t<
                       std::is_base_of_v<typed_actor_view_base, T>>>
  explicit typed_actor(T ptr) : ptr_(ptr.ctrl()) {
    static_assert(
      detail::tl_subset_of<signatures, typename T::signatures>::value,
      "Cannot assign T to incompatible handle type");
  }

  template <class... Ts>
  typed_actor& operator=(const typed_actor<Ts...>& other) {
    static_assert(
      detail::tl_subset_of<signatures, detail::type_list<Ts...>>::value,
      "Cannot assign incompatible handle");
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
    return {ptr_.get(), true};
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

  /// @cond PRIVATE

  abstract_actor* operator->() const noexcept {
    return ptr_->get();
  }

  abstract_actor& operator*() const noexcept {
    return *ptr_->get();
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

  typed_actor(actor_control_block* ptr, bool add_ref) : ptr_(ptr, add_ref) {
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
    return inspect(f, x.ptr_);
  }

  /// Releases the reference held by handle `x`. Using the
  /// handle after invalidating it is undefined behavior.
  friend void destroy(typed_actor& x) {
    x.ptr_.reset();
  }

  static std::array<type_id_list, sizeof...(Sigs)> allowed_inputs() {
    return {{detail::make_argument_type_id_list<Sigs>()...}};
  }

  /// @endcond

private:
  actor_control_block* get() const noexcept {
    return ptr_.get();
  }

  actor_control_block* release() noexcept {
    return ptr_.release();
  }

  typed_actor(actor_control_block* ptr) : ptr_(ptr) {
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

CAF_PUSH_DEPRECATED_WARNING

/// Returns a new actor that implements the composition `f.g(x) = f(g(x))`.
/// @relates typed_actor
template <class... Xs, class... Ys>
[[deprecated]] composed_type_t<detail::type_list<Xs...>,
                               detail::type_list<Ys...>>
operator*(typed_actor<Xs...> f, typed_actor<Ys...> g) {
  using result
    = composed_type_t<detail::type_list<Xs...>, detail::type_list<Ys...>>;
  auto& sys = g->home_system();
  auto mts = sys.message_types(detail::type_list<result>{});
  return make_actor<decorator::sequencer, result>(
    sys.next_actor_id(), sys.node(), &sys,
    actor_cast<strong_actor_ptr>(std::move(f)),
    actor_cast<strong_actor_ptr>(std::move(g)), std::move(mts));
}

CAF_POP_WARNINGS

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
