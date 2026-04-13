// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/attachable.hpp"

#include <type_traits>

namespace caf::detail {

template <class F>
class functor_attachable : public attachable {
public:
  static constexpr bool is_fn_0 = std::is_invocable_v<F>;

  static constexpr bool is_fn_1 = std::is_invocable_v<F, const error&>;

  static constexpr bool is_fn_2
    = std::is_invocable_v<F, const error&, scheduler*>;

  static_assert(is_fn_0 || is_fn_1 || is_fn_2,
                "F must be a function or a function object "
                "with 0, 1 or 2 arguments");

  static_assert((is_fn_0 ? 1 : 0) + (is_fn_1 ? 1 : 0) + (is_fn_2 ? 1 : 0) == 1,
                "F may not accept multiple signatures");

  explicit functor_attachable(F fn) : fn_(std::move(fn)) {
    // nop
  }

  void actor_exited(abstract_actor*, const error& fail_state,
                    [[maybe_unused]] scheduler* sched) override {
    if constexpr (is_fn_0) {
      fn_();
    } else if constexpr (is_fn_1) {
      fn_(fail_state);
    } else {
      fn_(fail_state, sched);
    }
  }

private:
  F fn_;
};

} // namespace caf::detail
