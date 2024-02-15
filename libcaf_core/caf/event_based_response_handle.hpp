// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/message_id.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf {

/// This helper class identifies an expected response message and enables
/// `request(...).then(...)`.
template <class Result>
class event_based_response_handle {
public:
  // -- constructors, destructors, and assignment operators --------------------

  event_based_response_handle(scheduled_actor* self, message_id mid)
    : self_(self), mid_(mid) {
    // nop
  }

  template <class OnValue, class OnError>
  void await(OnValue on_value, OnError on_error) && {
    type_check<OnValue, OnError>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    self_->add_awaited_response_handler(mid_, std::move(bhvr));
  }

  template <class OnValue>
  void await(OnValue on_value) && {
    return std::move(*this).await(std::move(on_value),
                                  [self = self_](error& err) {
                                    self->call_error_handler(err);
                                  });
  }

  template <class OnValue, class OnError>
  void then(OnValue on_value, OnError on_error) && {
    type_check<OnValue, OnError>();
    auto bhvr = behavior{std::move(on_value), std::move(on_error)};
    self_->add_multiplexed_response_handler(mid_, std::move(bhvr));
  }

  template <class OnValue>
  void then(OnValue on_value) && {
    return std::move(*this).then(std::move(on_value),
                                 [self = self_](error& err) {
                                   self->call_error_handler(err);
                                 });
  }

private:
  template <class OnValue, class OnError>
  static constexpr void type_check() {
    // Type-check OnValue.
    using on_value_trait_helper = typename detail::get_callable_trait<OnValue>;
    static_assert(on_value_trait_helper::valid,
                  "OnValue must provide a single, non-template operator()");
    using on_value_trait = typename on_value_trait_helper::type;
    static_assert(std::is_same_v<typename on_value_trait::result_type, void>,
                  "OnValue must return void");
    using on_value_args = typename on_value_trait::decayed_arg_types;
    if constexpr (!std::is_same_v<Result, message>) {
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
};

} // namespace caf
