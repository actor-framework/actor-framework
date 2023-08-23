// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/const_typed_message_view.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/invoke_result_visitor.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"
#include "caf/ref_counted.hpp"
#include "caf/response_promise.hpp"
#include "caf/skip.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/timespan.hpp"
#include "caf/typed_message_view.hpp"
#include "caf/typed_response_promise.hpp"

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace caf {

class message_handler;

} // namespace caf

namespace caf::detail {

class CAF_CORE_EXPORT behavior_impl : public ref_counted {
public:
  using pointer = intrusive_ptr<behavior_impl>;

  ~behavior_impl() override;

  behavior_impl();

  explicit behavior_impl(timespan tout);

  bool invoke_empty(detail::invoke_result_visitor& f);

  virtual bool invoke(detail::invoke_result_visitor& f, message& xs) = 0;

  std::optional<message> invoke(message&);

  virtual void handle_timeout();

  timespan timeout() const noexcept {
    return timeout_;
  }

  pointer or_else(const pointer& other);

protected:
  timespan timeout_;
};

template <bool HasTimeout, class Tuple>
struct with_generic_timeout;

template <class... Ts>
struct with_generic_timeout<false, std::tuple<Ts...>> {
  using type = std::tuple<Ts..., generic_timeout_definition>;
};

template <class... Ts>
struct with_generic_timeout<true, std::tuple<Ts...>> {
  using type = tl_apply_t<
    tl_replace_back_t<type_list<Ts...>, generic_timeout_definition>,
    std::tuple>;
};

struct dummy_timeout_definition {
  timespan timeout = infinite;

  constexpr void handler() {
    // nop
  }
};

template <class Tuple, class TimeoutDefinition = dummy_timeout_definition>
class default_behavior_impl;

template <class... Ts, class TimeoutDefinition>
class default_behavior_impl<std::tuple<Ts...>, TimeoutDefinition>
  : public behavior_impl {
public:
  using super = behavior_impl;

  using tuple_type = std::tuple<Ts...>;

  default_behavior_impl(tuple_type&& tup, TimeoutDefinition timeout_definition)
    : super(timeout_definition.timeout),
      cases_(std::move(tup)),
      timeout_definition_(std::move(timeout_definition)) {
    // nop
  }

  virtual bool invoke(detail::invoke_result_visitor& f, message& xs) override {
    return invoke_impl(f, xs, std::make_index_sequence<sizeof...(Ts)>{});
  }

  template <size_t... Is>
  bool invoke_impl(detail::invoke_result_visitor& f, message& msg,
                   std::index_sequence<Is...>) {
    [[maybe_unused]] auto dispatch = [&](auto& fun) {
      using fun_type = std::decay_t<decltype(fun)>;
      using trait = get_callable_trait_t<fun_type>;
      auto arg_types = to_type_id_list<typename trait::decayed_arg_types>();
      if (arg_types == msg.types()) {
        typename trait::message_view_type xs{msg};
        using fun_result = decltype(detail::apply_args(fun, xs));
        if constexpr (std::is_same_v<void, fun_result>) {
          detail::apply_args(fun, xs);
          f(unit);
        } else {
          auto invoke_res = detail::apply_args(fun, xs);
          f(invoke_res);
        }
        return true;
      }
      return false;
    };
    return (dispatch(std::get<Is>(cases_)) || ...);
  }

  void handle_timeout() override {
    timeout_definition_.handler();
  }

private:
  tuple_type cases_;

  TimeoutDefinition timeout_definition_;
};

template <class TimeoutDefinition>
struct behavior_factory_t {
  TimeoutDefinition& tdef;

  template <class... Ts>
  auto operator()(Ts&... xs) {
    using impl = default_behavior_impl<std::tuple<Ts...>, TimeoutDefinition>;
    return make_counted<impl>(std::make_tuple(std::move(xs)...),
                              std::move(tdef));
  }
};

struct make_behavior_t {
  constexpr make_behavior_t() {
    // nop
  }

  template <class... Ts>
  auto operator()(Ts... xs) const {
    if constexpr ((is_timeout_definition<Ts>::value || ...)) {
      auto args = std::tie(xs...);
      auto& tdef = std::get<sizeof...(Ts) - 1>(args);
      behavior_factory_t<std::decay_t<decltype(tdef)>> f{tdef};
      std::make_index_sequence<sizeof...(Ts) - 1> indexes;
      return detail::apply_args(f, indexes, args);
    } else {
      using type = default_behavior_impl<std::tuple<Ts...>>;
      dummy_timeout_definition dummy;
      return make_counted<type>(std::make_tuple(std::move(xs)...), dummy);
    }
  }
};

constexpr make_behavior_t make_behavior = make_behavior_t{};

using behavior_impl_ptr = intrusive_ptr<behavior_impl>;

// utility for getting a type-erased version of make_behavior
struct make_behavior_impl_t {
  constexpr make_behavior_impl_t() {
    // nop
  }

  template <class... Ts>
  behavior_impl_ptr operator()(Ts&&... xs) const {
    return make_behavior(std::forward<Ts>(xs)...);
  }
};

constexpr make_behavior_impl_t make_behavior_impl = make_behavior_impl_t{};

} // namespace caf::detail
