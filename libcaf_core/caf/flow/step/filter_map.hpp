// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"

#include <utility>

namespace caf::flow::step {

template <class F>
class filter_map {
public:
  using trait = detail::get_callable_trait_t<F>;

  static_assert(!std::is_same_v<typename trait::result_type, void>,
                "filter_map functions may not return void");

  static_assert(trait::num_args == 1,
                "filter_map functions must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using output_type = std::decay_t<typename trait::result_type::value_type>;

  explicit filter_map(F fn) : fn_(std::move(fn)) {
    // nop
  }

  filter_map(filter_map&&) = default;
  filter_map(const filter_map&) = default;
  filter_map& operator=(filter_map&&) = default;
  filter_map& operator=(const filter_map&) = default;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    auto mapped = fn_(item);
    return mapped ? next.on_next(std::move(*mapped), steps...) : true;
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
