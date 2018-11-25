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

#include <new>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/sec.hpp"

#include "caf/logger.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

/// An event-based actor with managed state. The state is constructed
/// before `make_behavior` will get called and destroyed after the
/// actor called `quit`. This state management brakes cycles and
/// allows actors to automatically release ressources as soon
/// as possible.
template <class State, class Base /* = event_based_actor (see fwd.hpp) */>
class stateful_actor : public Base {
public:
  template <class... Ts>
  explicit stateful_actor(actor_config& cfg, Ts&&... xs)
      : Base(cfg, std::forward<Ts>(xs)...),
        state(state_) {
    if (detail::is_serializable<State>::value)
      this->setf(Base::is_serializable_flag);
    cr_state(this);
  }

  ~stateful_actor() override {
    // nop
  }

  /// Destroys the state of this actor (no further overriding allowed).
  void on_exit() final {
    CAF_LOG_TRACE("");
    state_.~State();
  }

  const char* name() const final {
    return get_name(state_);
  }

  error save_state(serializer& sink, unsigned int version) override {
    return serialize_state(&sink, state, version);
  }

  error load_state(deserializer& source, unsigned int version) override {
    return serialize_state(&source, state, version);
  }

  /// A reference to the actor's state.
  State& state;

  /// @cond PRIVATE

  void initialize() override {
    Base::initialize();
  }

  /// @endcond

private:
  template <class Inspector, class T>
  auto serialize_state(Inspector* f, T& x, unsigned int)
  -> decltype(inspect(*f, x)) {
    return inspect(*f, x);
  }

  template <class T>
  error serialize_state(void*, T&, unsigned int) {
    return sec::invalid_argument;
  }

  template <class T>
  typename std::enable_if<std::is_constructible<State, T>::value>::type
  cr_state(T arg) {
    new (&state_) State(arg);
  }

  template <class T>
  typename std::enable_if<!std::is_constructible<State, T>::value>::type
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
  typename std::enable_if<!detail::has_name<U>::value, const char*>::type
  get_name(const U&) const {
    return Base::name();
  }

  union { State state_; };
};

} // namespace caf

