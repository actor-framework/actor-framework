// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"

#include <utility>

namespace caf::flow::step {

template <class F>
class reduce {
public:
  using trait = detail::get_callable_trait_t<F>;

  static_assert(!std::is_same_v<typename trait::result_type, void>,
                "reduce functions may not return void");

  static_assert(trait::num_args == 2,
                "reduce functions must take exactly two arguments");

  using arg_type = typename trait::arg_types;

  using input_type = std::decay_t<detail::tl_at_t<arg_type, 1>>;

  using output_type = std::decay_t<typename trait::result_type>;

  explicit reduce(output_type init, F fn)
    : val_(std::move(init)), fn_(std::move(fn)) {
    // nop
  }

  reduce(reduce&&) = default;
  reduce(const reduce&) = default;
  reduce& operator=(reduce&&) = default;
  reduce& operator=(const reduce&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next&, Steps&...) {
    val_ = fn_(std::move(val_), item);
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    if (next.on_next(val_, steps...))
      next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    if (next.on_next(val_, steps...))
      next.on_error(what, steps...);
  }

private:
  output_type val_;
  F fn_;
};

} // namespace caf::flow::step
