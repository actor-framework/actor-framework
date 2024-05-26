// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"

#include <utility>

namespace caf::flow::step {

template <class F>
class do_on_next {
public:
  using trait = detail::get_callable_trait_t<F>;

  static_assert(trait::num_args == 1,
                "do_on_next functions must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using output_type = input_type;

  explicit do_on_next(F fn) : fn_(std::move(fn)) {
    // nop
  }

  do_on_next(do_on_next&&) = default;
  do_on_next(const do_on_next&) = default;
  do_on_next& operator=(do_on_next&&) = default;
  do_on_next& operator=(const do_on_next&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    fn_(item);
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }

private:
  F fn_;
};

} // namespace caf::flow::step
