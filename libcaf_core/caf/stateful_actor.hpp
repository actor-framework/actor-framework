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

#ifndef CAF_STATEFUL_ACTOR_HPP
#define CAF_STATEFUL_ACTOR_HPP

#include <new>
#include <type_traits>

#include "caf/fwd.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

template <class Archive, class U>
typename std::enable_if<detail::is_serializable<U>::value>::type
serialize_state(Archive& ar, U& st, const unsigned int version) {
  serialize(ar, st, version);
}

template <class Archive, class U>
typename std::enable_if<! detail::is_serializable<U>::value>::type
serialize_state(Archive&, U&, const unsigned int) {
  throw std::logic_error("serialize_state with unserializable type called");
}

/// An event-based actor with managed state. The state is constructed
/// before `make_behavior` will get called and destroyed after the
/// actor called `quit`. This state management brakes cycles and
/// allows actors to automatically release ressources as soon
/// as possible.
template <class State, class Base = event_based_actor>
class stateful_actor : public Base {
public:
  template <class... Ts>
  stateful_actor(Ts&&... xs) : Base(std::forward<Ts>(xs)...), state(state_) {
    if (detail::is_serializable<State>::value)
      this->is_serializable(true);
  }

  ~stateful_actor() {
    // nop
  }

  /// Destroys the state of this actor (no further overriding allowed).
  void on_exit() override final {
    CAF_LOG_TRACE("");
    state_.~State();
  }

  const char* name() const override final {
    return get_name(state_);
  }

  void save_state(serializer& sink, const unsigned int version) override {
    serialize_state(sink, state, version);
  }

  void load_state(deserializer& source, const unsigned int version) override {
    serialize_state(source, state, version);
  }

  /// A reference to the actor's state.
  State& state;

  /// @cond PRIVATE

  void initialize() override {
    cr_state(this);
    Base::initialize();
  }

  /// @endcond

private:
  template <class T>
  typename std::enable_if<std::is_constructible<State, T>::value>::type
  cr_state(T arg) {
    new (&state_) State(arg);
  }

  template <class T>
  typename std::enable_if<! std::is_constructible<State, T>::value>::type
  cr_state(T) {
    new (&state_) State();
  }

  static const char* unbox_str(const char* str) {
    return str;
  }

  template <class U>
  static const char* unbox_str(const U& str) {
    return str.c_str();
  }

  template <class U>
  typename std::enable_if<detail::has_name<U>::value, const char*>::type
  get_name(const U& st) const {
    return unbox_str(st.name);
  }

  template <class U>
  typename std::enable_if<! detail::has_name<U>::value, const char*>::type
  get_name(const U&) const {
    return Base::name();
  }

  union { State state_; };
};

} // namespace caf

#endif // CAF_STATEFUL_ACTOR_HPP
