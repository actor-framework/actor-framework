// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/attachable.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf::detail {

template <class F,
          int Args = tl_size<typename get_callable_trait<F>::arg_types>::value>
struct functor_attachable : attachable {
  static_assert(Args == 1 || Args == 2,
                "Only 0, 1 or 2 arguments for F are supported");
  F functor_;
  functor_attachable(F arg) : functor_(std::move(arg)) {
    // nop
  }
  void actor_exited(const error& fail_state, execution_unit*) override {
    functor_(fail_state);
  }
  static constexpr size_t token_type = attachable::token::anonymous;
};

template <class F>
struct functor_attachable<F, 2> : attachable {
  F functor_;
  functor_attachable(F arg) : functor_(std::move(arg)) {
    // nop
  }
  void actor_exited(const error& x, execution_unit* y) override {
    functor_(x, y);
  }
};

template <class F>
struct functor_attachable<F, 0> : attachable {
  F functor_;
  functor_attachable(F arg) : functor_(std::move(arg)) {
    // nop
  }
  void actor_exited(const error&, execution_unit*) override {
    functor_();
  }
};

} // namespace caf::detail
