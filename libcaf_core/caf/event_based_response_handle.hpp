// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/response_handle_expected_helper.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/message_id.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::detail {

template <class T>
struct event_based_response_handle_res {
  using type = T;
};

template <class T>
struct event_based_response_handle_res<type_list<T>> {
  using type = T;
};

template <class T0, class T1, class... Ts>
struct event_based_response_handle_res<type_list<T0, T1, Ts...>> {};

template <>
struct event_based_response_handle_res<message> {};

template <class T>
using event_based_response_handle_res_t =
  typename event_based_response_handle_res<T>::type;

} // namespace caf::detail

namespace caf {

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class Result>
class event_based_response_handle {
public:
  // -- constructors, destructors, and assignment operators --------------------

  event_based_response_handle(scheduled_actor* self, message_id mid,
                              disposable pending_timeout)
    : self_(self), mid_(mid), pending_timeout_(std::move(pending_timeout)) {
    // nop
  }

  template <class OnValue, class OnError>
  void await(OnValue on_value, OnError on_error) && {
    type_check<OnValue, OnError>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    self_->add_awaited_response_handler(mid_, std::move(bhvr),
                                        std::move(pending_timeout_));
  }

  template <class OnValue>
  void await(OnValue on_value) && {
    using helper = detail::response_handle_expected_helper<Result, OnValue>;
    if constexpr (helper::use_expected) {
      auto bhvr = behavior{helper::on_value(on_value),
                           helper::on_error(on_value)};
      self_->add_awaited_response_handler(mid_, std::move(bhvr),
                                          std::move(pending_timeout_));

    } else {
      std::move(*this).await(std::move(on_value), [self = self_](error& err) {
        self->call_error_handler(err);
      });
    }
  }

  template <class OnValue, class OnError>
  void then(OnValue on_value, OnError on_error) && {
    type_check<OnValue, OnError>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    self_->add_multiplexed_response_handler(mid_, std::move(bhvr),
                                            std::move(pending_timeout_));
  }

  template <class OnValue>
  void then(OnValue on_value) && {
    using helper = detail::response_handle_expected_helper<Result, OnValue>;
    if constexpr (helper::use_expected) {
      auto bhvr = behavior{helper::on_value(on_value),
                           helper::on_error(on_value)};
      self_->add_multiplexed_response_handler(mid_, std::move(bhvr),
                                              std::move(pending_timeout_));

    } else {
      std::move(*this).then(std::move(on_value), [self = self_](error& err) {
        self->call_error_handler(err);
      });
    }
  }

  template <class T = Result, class Self = scheduled_actor>
  flow::assert_scheduled_actor_hdr_t<
    flow::observable<detail::event_based_response_handle_res_t<T>>>
  as_observable() && {
    using res_t = detail::event_based_response_handle_res_t<T>;
    return static_cast<Self*>(self_)
      ->template single_from_response<res_t>(mid_, std::move(pending_timeout_))
      .as_observable();
  }

private:
  template <class OnValue, class OnError>
  static constexpr void type_check() {
    // Type-check OnValue.
    using on_value_trait_helper = detail::get_callable_trait<OnValue>;
    static_assert(on_value_trait_helper::valid,
                  "OnValue must provide a single, non-template operator()");
    using on_value_trait = typename on_value_trait_helper::type;
    static_assert(std::is_same_v<typename on_value_trait::result_type, void>,
                  "OnValue must return void");
    if constexpr (!std::is_same_v<Result, message>) {
      using on_value_args = typename on_value_trait::decayed_arg_types;
      static_assert(std::is_same_v<on_value_args, Result>,
                    "OnValue does not match the expected response types");
    }
    // Type-check OnError.
    using on_error_trait_helper = typename detail::get_callable_trait<OnError>;
    static_assert(on_error_trait_helper::valid,
                  "OnError must provide a single, non-template operator()");
    using on_error_trait = typename on_error_trait_helper::type;
    static_assert(std::is_same_v<typename on_error_trait::result_type, void>,
                  "OnError must return void");
    using on_error_args = typename on_error_trait::decayed_arg_types;
    static_assert(std::is_same_v<on_error_args, type_list<error>>,
                  "OnError must accept a single argument of type caf::error");
  }

  /// Points to the parent actor.
  scheduled_actor* self_;

  /// Stores the ID of the message we are waiting for.
  message_id mid_;

  /// Stores a handle to the in-flight timeout.
  disposable pending_timeout_;
};

} // namespace caf
