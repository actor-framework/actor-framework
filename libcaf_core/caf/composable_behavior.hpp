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

#ifndef CAF_COMPOSABLE_STATE_HPP
#define CAF_COMPOSABLE_STATE_HPP

#include "caf/param.hpp"
#include "caf/behavior.hpp"
#include "caf/replies_to.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_actor_pointer.hpp"
#include "caf/abstract_composable_behavior.hpp"

namespace caf {

/// Generates an interface class that provides `operator()`. The signature
/// of the apply operator is derived from the typed message passing interface
/// `MPI`.
template <class MPI>
class abstract_composable_behavior_mixin;

template <class... Xs, class... Ys>
class abstract_composable_behavior_mixin<typed_mpi<detail::type_list<Xs...>,
                                                detail::type_list<Ys...>>> {
public:
  virtual ~abstract_composable_behavior_mixin() noexcept {
    // nop
  }

  virtual result<Ys...> operator()(param_t<Xs>...) = 0;
};

// this class works around compiler issues on GCC
template <class... Ts>
struct abstract_composable_behavior_mixin_helper;

template <class T, class... Ts>
struct abstract_composable_behavior_mixin_helper<T, Ts...>
  : public abstract_composable_behavior_mixin<T>,
    public abstract_composable_behavior_mixin_helper<Ts...> {
  using abstract_composable_behavior_mixin<T>::operator();
  using abstract_composable_behavior_mixin_helper<Ts...>::operator();
};

template <class T>
struct abstract_composable_behavior_mixin_helper<T>
  : public abstract_composable_behavior_mixin<T> {
  using abstract_composable_behavior_mixin<T>::operator();
};

template <class T, class... Fs>
void init_behavior_impl(T*, detail::type_list<>, behavior& storage, Fs... fs) {
  storage.assign(std::move(fs)...);
}

template <class T, class... Xs, class... Ys, class... Ts, class... Fs>
void init_behavior_impl(T* thisptr,
                        detail::type_list<typed_mpi<detail::type_list<Xs...>,
                                                    detail::type_list<Ys...>>,
                                          Ts...>,
                        behavior& storage, Fs... fs) {
  auto f = [=](param_t<Xs>... xs) {
    return (*thisptr)(std::move(xs)...);
  };
  detail::type_list<Ts...> token;
  init_behavior_impl(thisptr, token, storage, fs..., f);
}

/// Base type for composable actor states.
template <class TypedActor>
class composable_behavior;

template <class... Clauses>
class composable_behavior<typed_actor<Clauses...>>
  : virtual public abstract_composable_behavior,
    public abstract_composable_behavior_mixin_helper<Clauses...> {
public:
  using signatures = detail::type_list<Clauses...>;

  using handle_type =
    typename detail::tl_apply<
      signatures,
      typed_actor
    >::type;

  using actor_base = typename handle_type::base;

  using behavior_type = typename handle_type::behavior_type;

  composable_behavior() : self(nullptr) {
    // nop
  }

  template <class SelfPointer>
  void init_selfptr(SelfPointer selfptr) {
    self = selfptr;
  }

  void init_behavior(behavior& x) override {
    signatures token;
    init_behavior_impl(this, token, x);
  }

  typed_actor_pointer<Clauses...> self;
};

} // namespace caf

#endif // CAF_COMPOSABLE_STATE_HPP
