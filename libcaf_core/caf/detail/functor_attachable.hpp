// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/attachable.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf::detail {

template <class F>
class functor_attachable : public attachable {
public:
  static constexpr size_t num_args
    = tl_size<typename get_callable_trait<F>::arg_types>::value;

  static_assert(num_args < 3, "Only 0, 1 or 2 arguments for F are supported");

  explicit functor_attachable(F fn) : fn_(std::move(fn)) {
    // nop
  }

  void actor_exited(const error& fail_state, execution_unit* host) override {
    if constexpr (num_args == 0)
      fn_();
    else if constexpr (num_args == 1)
      fn_(fail_state);
    else
      fn_(fail_state, host);
  }

  static constexpr size_t token_type = attachable::token::anonymous;

private:
  F fn_;
};

} // namespace caf::detail
