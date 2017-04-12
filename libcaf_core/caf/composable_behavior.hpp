/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
class composable_behavior_base;

template <class... Xs, class... Ys>
class composable_behavior_base<typed_mpi<detail::type_list<Xs...>,
                                         output_tuple<Ys...>>> {
public:
  virtual ~composable_behavior_base() noexcept {
    // nop
  }

  virtual result<Ys...> operator()(param_t<Xs>...) = 0;

  // C++14 and later
# if __cplusplus > 201103L
  auto make_callback() {
    return [=](param_t<Xs>... xs) { return (*this)(std::move(xs)...); };
  }
# else
  // C++11
  std::function<result<Ys...> (param_t<Xs>...)> make_callback() {
    return [=](param_t<Xs>... xs) { return (*this)(std::move(xs)...); };
  }
# endif
};

/// Base type for composable actor states.
template <class TypedActor>
class composable_behavior;

template <class... Clauses>
class composable_behavior<typed_actor<Clauses...>>
  : virtual public abstract_composable_behavior,
    public composable_behavior_base<Clauses>... {
public:
  using signatures = detail::type_list<Clauses...>;

  using handle_type =
    typename detail::tl_apply<
      signatures,
      typed_actor
    >::type;

  using actor_base = typename handle_type::base;

  using broker_base = typename handle_type::broker_base;

  using behavior_type = typename handle_type::behavior_type;

  composable_behavior() : self(nullptr) {
    // nop
  }

  template <class SelfPointer>
  unit_t init_selfptr(SelfPointer x) {
    CAF_ASSERT(x != nullptr);
    self = x;
    return unit;
  }

  void init_behavior(message_handler& x) override {
    init_behavior_impl(x);
  }

  unit_t init_behavior_impl(message_handler& x) {
    if (x)
      x = x.or_else(composable_behavior_base<Clauses>::make_callback()...);
    else
      x.assign(composable_behavior_base<Clauses>::make_callback()...);
    return unit;
  }

  typed_actor_pointer<Clauses...> self;
};

} // namespace caf

#endif // CAF_COMPOSABLE_STATE_HPP
