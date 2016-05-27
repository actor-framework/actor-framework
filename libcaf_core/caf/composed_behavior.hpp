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

#ifndef CAF_COMPOSED_STATE_HPP
#define CAF_COMPOSED_STATE_HPP

#include "caf/param.hpp"
#include "caf/composable_behavior.hpp"
#include "caf/typed_actor_pointer.hpp"

namespace caf {

template <class InterfaceIntersection, class... States>
class composed_behavior_base;

template <class T>
class composed_behavior_base<detail::type_list<>, T> : public T {
public:
  using T::operator();

  // make this pure again, since the compiler can otherwise
  // runs into a "multiple final overriders" error
  virtual void init_behavior(behavior& x) override = 0;
};

template <class A, class B, class... Ts>
class composed_behavior_base<detail::type_list<>, A, B, Ts...>
  : public A,
    public composed_behavior_base<detail::type_list<>, B, Ts...> {
public:
  using super = composed_behavior_base<detail::type_list<>, B, Ts...>;

  using A::operator();
  using super::operator();

  template <class SelfPtr>
  void init_selfptr(SelfPtr ptr) {
    A::init_selfptr(ptr);
    super::init_selfptr(ptr);
  }

  // make this pure again, since the compiler can otherwise
  // runs into a "multiple final overriders" error
  virtual void init_behavior(behavior& x) override = 0;
};

template <class... Xs, class... Ys, class... Ts, class... States>
class composed_behavior_base<detail::type_list<typed_mpi<detail::type_list<Xs...>,
                                              detail::type_list<Ys...>>,
                                    Ts...>,
                          States...>
  : public composed_behavior_base<detail::type_list<Ts...>, States...> {
public:
  using super = composed_behavior_base<detail::type_list<Ts...>, States...>;

  using super::operator();

  virtual result<Ys...> operator()(param_t<Xs>...) override = 0;
};

template <class... Ts>
class composed_behavior
  : public composed_behavior_base<typename detail::tl_intersect<
                                 typename Ts::signatures...
                               >::type,
                               Ts...> {
private:
  using super =
    composed_behavior_base<typename detail::tl_intersect<
                          typename Ts::signatures...
                        >::type,
                        Ts...>;

public:
  using signatures =
    typename detail::tl_union<typename Ts::signatures...>::type;

  using handle_type =
    typename detail::tl_apply<
      signatures,
      typed_actor
    >::type;

  using behavior_type = typename handle_type::behavior_type;

  using combined_type = composed_behavior;

  using actor_base = typename handle_type::base;

  using self_pointer =
    typename detail::tl_apply<
      signatures,
      typed_actor_pointer
    >::type;

  composed_behavior() : self(nullptr) {
    // nop
  }

  template <class SelfPtr>
  void init_selfptr(SelfPtr ptr) {
    self = ptr;
    super::init_selfptr(ptr);
  }

  using super::operator();

  void init_behavior(behavior& x) override {
    signatures token;
    init_behavior_impl(this, token, x);
  }

protected:
  self_pointer self;
};

} // namespace caf

#endif // CAF_COMPOSED_STATE_HPP
