/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "caf/const_typed_message_view.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/invoke_result_visitor.hpp"
#include "caf/detail/param_message_view.hpp"
#include "caf/detail/tail_argument_token.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"
#include "caf/match_result.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"
#include "caf/ref_counted.hpp"
#include "caf/response_promise.hpp"
#include "caf/skip.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/timespan.hpp"
#include "caf/typed_message_view.hpp"
#include "caf/typed_response_promise.hpp"
#include "caf/variant.hpp"

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

  match_result invoke_empty(detail::invoke_result_visitor& f);

  virtual match_result invoke(detail::invoke_result_visitor& f, message& xs)
    = 0;

  optional<message> invoke(message&);

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
  using type =
    typename tl_apply<typename tl_replace_back<
                        type_list<Ts...>, generic_timeout_definition>::type,
                      std::tuple>::type;
};

template <class Tuple>
class default_behavior_impl;

template <class... Ts>
class default_behavior_impl<std::tuple<Ts...>> : public behavior_impl {
public:
  using tuple_type = std::tuple<Ts...>;

  using back_type = typename tl_back<type_list<Ts...>>::type;

  static constexpr bool has_timeout = is_timeout_definition<back_type>::value;

  static constexpr size_t num_cases = sizeof...(Ts) - (has_timeout ? 1 : 0);

  default_behavior_impl(tuple_type&& tup) : cases_(std::move(tup)) {
    if constexpr (has_timeout) {
      this->timeout_ = std::get<num_cases>(cases_).timeout;
    }
  }

  template <class... Us>
  default_behavior_impl(Us&&... xs) : cases_(std::forward<Us>(xs)...) {
    if constexpr (has_timeout) {
      this->timeout_ = std::get<num_cases>(cases_).timeout;
    }
  }

  virtual match_result invoke(detail::invoke_result_visitor& f,
                              message& xs) override {
    return invoke_impl(f, xs, std::make_index_sequence<sizeof...(Ts)>{});
  }

  template <size_t... Is>
  match_result invoke_impl(detail::invoke_result_visitor& f, message& msg,
                           std::index_sequence<Is...>) {
    auto result = match_result::no_match;
    auto dispatch = [&](auto& fun) {
      using fun_type = std::decay_t<decltype(fun)>;
      if constexpr (!is_timeout_definition<fun_type>::value) {
        using trait = get_callable_trait_t<fun_type>;
        auto arg_types = to_type_id_list<typename trait::decayed_arg_types>();
        if (arg_types == msg.types()) {
          typename trait::message_view_type xs{msg};
          using fun_result = decltype(detail::apply_args(fun, xs));
          if constexpr (std::is_same_v<void, fun_result>) {
            detail::apply_args(fun, xs);
            result = f.visit(unit) ? match_result::match : match_result::skip;
          } else {
            auto invoke_res = detail::apply_args(fun, xs);
            result = f.visit(invoke_res) ? match_result::match
                                         : match_result::skip;
          }
          return true;
        }
      }
      return false;
    };
    static_cast<void>((dispatch(std::get<Is>(cases_)) || ...));
    return result;
  }

  void handle_timeout() override {
    if constexpr (has_timeout) {
      std::get<num_cases>(cases_).handler();
    }
  }

private:
  tuple_type cases_;
};

template <class Tuple>
struct behavior_factory {
  template <class... Ts>
  typename behavior_impl::pointer operator()(Ts&&... xs) const {
    return make_counted<default_behavior_impl<Tuple>>(std::forward<Ts>(xs)...);
  }
};

struct make_behavior_t {
  constexpr make_behavior_t() {
    // nop
  }

  template <class... Ts>
  intrusive_ptr<default_behavior_impl<std::tuple<Ts...>>>
  operator()(Ts... xs) const {
    using type = default_behavior_impl<std::tuple<Ts...>>;
    return make_counted<type>(std::move(xs)...);
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
