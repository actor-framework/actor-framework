/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_TYPED_ACTOR_HPP
#define CAF_TYPED_ACTOR_HPP

#include "caf/intrusive_ptr.hpp"

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/replies_to.hpp"
#include "caf/bound_actor.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/composed_actor.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/typed_response_promise.hpp"

#include "caf/detail/mpi_bind.hpp"
#include "caf/detail/mpi_composition.hpp"

namespace caf {

template <class... Sigs>
class typed_event_based_actor;

namespace io {
namespace experimental {

template <class... Sigs>
class typed_broker;

} // namespace experimental
} // namespace io

/// Identifies a statically typed actor.
/// @tparam Sigs Signature of this actor as `replies_to<...>::with<...>`
///              parameter pack.
template <class... Sigs>
class typed_actor : detail::comparable<typed_actor<Sigs...>>,
                    detail::comparable<typed_actor<Sigs...>, actor_addr>,
                    detail::comparable<typed_actor<Sigs...>, invalid_actor_t>,
                    detail::comparable<typed_actor<Sigs...>,
                                       invalid_actor_addr_t> {
 public:
  static_assert(sizeof...(Sigs) > 0, "Empty typed actor handle");

  // grant access to private ctor
  friend class local_actor;

  // friend with all possible instantiations
  template <class...>
  friend class typed_actor;

  // allow conversion via actor_cast
  template <class>
  friend struct actor_cast_access;

  /// Creates a new `typed_actor` type by extending this one with `Es...`.
  template <class... Es>
  using extend = typed_actor<Sigs..., Es...>;

  /// Creates a new `typed_actor` type by extending this one with another
  /// `typed_actor`.
  template <class T>
  using extend_with =
    typename detail::extend_with_helper<T, Sigs...>::type;

  /// Identifies the behavior type actors of this kind use
  /// for their behavior stack.
  using behavior_type = typed_behavior<Sigs...>;

  /// Identifies pointers to instances of this kind of actor.
  using pointer = typed_event_based_actor<Sigs...>*;

  /// Identifies the base class for this kind of actor.
  using base = typed_event_based_actor<Sigs...>;

  /// Identifies pointers to brokers implementing this interface.
  using broker_pointer = io::experimental::typed_broker<Sigs...>*;

  /// Identifies the base class of brokers implementing this interface.
  using broker_base = io::experimental::typed_broker<Sigs...>;

  /// Stores the template parameter pack.
  using signatures = detail::type_list<Sigs...>;

  /// Identifies the base class for this kind of actor with actor.
  template <class State>
  using stateful_base = stateful_actor<State, base>;

  /// Identifies the base class for this kind of actor with actor.
  template <class State>
  using stateful_pointer = stateful_actor<State, base>*;

  /// Identifies the broker_base class for this kind of actor with actor.
  template <class State>
  using stateful_broker_base =
    stateful_actor<State, broker_base>;

  /// Identifies the broker_base class for this kind of actor with actor.
  template <class State>
  using stateful_broker_pointer =
    stateful_actor<State, broker_base>*;

  typed_actor() = default;
  typed_actor(typed_actor&&) = default;
  typed_actor(const typed_actor&) = default;
  typed_actor& operator=(typed_actor&&) = default;
  typed_actor& operator=(const typed_actor&) = default;

  template <class TypedActor,
            class Enable =
              typename std::enable_if<
                detail::tlf_is_subset(signatures(),
                                      typename TypedActor::signatures())
              >::type>
  typed_actor(const TypedActor& other) : ptr_(other.ptr_) {
    // nop
  }

  // allow `handle_type{this}` for typed actors
  template <class TypedActor,
            class Enable =
              typename std::enable_if<
                detail::tlf_is_subset(signatures(),
                                      typename TypedActor::signatures())
              >::type>
  typed_actor(TypedActor* ptr) : ptr_(ptr) {
    // nop
  }

  template <class TypedActor,
            class Enable =
              typename std::enable_if<
                detail::tlf_is_subset(signatures(),
                                      typename TypedActor::signatures())
              >::type>
  typed_actor& operator=(const TypedActor& other) {
    ptr_ = other.ptr_;
    return *this;
  }

  template <class Impl,
            class Enable =
              typename std::enable_if<
                detail::tlf_is_subset(signatures(),
                                      typename Impl::signatures())
              >::type>
  typed_actor(intrusive_ptr<Impl> other) : ptr_(std::move(other)) {
    // nop
  }

  typed_actor(const invalid_actor_t&) {
    // nop
  }

  typed_actor& operator=(const invalid_actor_t&) {
    ptr_.reset();
  }


  /// Queries the address of the stored actor.
  actor_addr address() const noexcept {
    return ptr_ ? ptr_->address() : actor_addr();
  }

  /// Returns `*this != invalid_actor`.
  explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

  /// Returns `*this == invalid_actor`.
  bool operator!() const noexcept {
    return !ptr_;
  }

  /// Returns the origin node of this actor.
  node_id node() const noexcept {
    return ptr_ ? ptr_->node() : invalid_node_id;
  }

  /// Returns the ID of this actor.
  actor_id id() const noexcept {
    return ptr_ ? ptr_->id() : invalid_actor_id;
  }

  /// Exchange content of `*this` and `other`.
  void swap(actor& other) noexcept {
    ptr_.swap(other.ptr_);
  }

  template <class... Ts>
  typename detail::mpi_bind<
    caf::typed_actor,
    detail::type_list<Sigs...>,
    typename std::decay<Ts>::type...
  >::type
  bind(Ts&&... xs) const {
    if (! ptr_)
      return invalid_actor;
    auto ptr = make_counted<bound_actor>(ptr_->address(),
                                         make_message(xs...));
    return {ptr.release(), false};
  }

  /// @cond PRIVATE

  abstract_actor* operator->() const noexcept {
    return ptr_.get();
  }

  abstract_actor& operator*() const noexcept {
    return *ptr_.get();
  }

  intptr_t compare(const typed_actor& x) const noexcept {
    return actor_addr::compare(get(), x.get());
  }

  intptr_t compare(const actor_addr& x) const noexcept {
    return actor_addr::compare(get(), actor_cast<abstract_actor*>(x));
  }

  intptr_t compare(const invalid_actor_t&) const noexcept {
    return ptr_ ? 1 : 0;
  }

  intptr_t compare(const invalid_actor_addr_t&) const noexcept {
    return ptr_ ? 1 : 0;
  }

  /// @endcond

private:
  abstract_actor* get() const noexcept {
    return ptr_.get();
  }

  abstract_actor* release() noexcept {
    return ptr_.release();
  }

  typed_actor(abstract_actor* ptr) : ptr_(ptr) {
    // nop
  }

  typed_actor(abstract_actor* ptr, bool add_ref) : ptr_(ptr, add_ref) {
    // nop
  }

  abstract_actor_ptr ptr_;
};

/// @relates typed_actor
template <class... Xs, class... Ys>
bool operator==(const typed_actor<Xs...>& x,
                const typed_actor<Ys...>& y) noexcept {
  return actor_addr::compare(actor_cast<abstract_actor*>(x),
                             actor_cast<abstract_actor*>(y)) == 0;
}

/// @relates typed_actor
template <class... Xs, class... Ys>
bool operator!=(const typed_actor<Xs...>& x,
                const typed_actor<Ys...>& y) noexcept {
  return ! (x == y);
}

/// Returns a new actor that implements the composition `f.g(x) = f(g(x))`.
/// @relates typed_actor
template <class... Xs, class... Ys>
typename detail::mpi_composition<
  typed_actor,
  detail::type_list<Xs...>,
  Ys...
>::type
operator*(typed_actor<Xs...> f, typed_actor<Ys...> g) {
  using result =
    typename detail::mpi_composition<
      typed_actor,
      detail::type_list<Xs...>,
      Ys...
    >::type;
  auto ptr = make_counted<composed_actor>(f.address(),
                                          g.address(),
                                          g->home_system().message_types(
                                            result{}));
  return actor_cast<result>(std::move(ptr));
}

/// @relates typed_actor
template <class T, class... Ts>
typename std::enable_if<T::is_saving::value>::type
serialize(T& sink, typed_actor<Ts...>& hdl, const unsigned int) {
  auto addr = hdl.address();
  sink << addr;
}

/// @relates typed_actor
template <class T, class... Ts>
typename std::enable_if<T::is_loading::value>::type
serialize(T& sink, typed_actor<Ts...>& hdl, const unsigned int) {
  actor_addr addr;
  sink >> addr;
  hdl = actor_cast<typed_actor<Ts...>>(addr);
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

#endif // CAF_TYPED_ACTOR_HPP
