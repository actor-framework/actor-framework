// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_cast.hpp"
#include "caf/delegated.hpp"
#include "caf/detail/send_type_check.hpp"
#include "caf/message.hpp"
#include "caf/message_priority.hpp"
#include "caf/response_promise.hpp"
#include "caf/response_type.hpp"
#include <type_traits>
#include "caf/detail/type_list.hpp"

namespace caf {

/// Provides a fluent interface for sending messages through a response promise.
template <message_priority Priority, class Outputs, class... Inputs>
class [[nodiscard]] response_promise_mail_t {
public:
  response_promise_mail_t(response_promise& rp, message&& content)
    : rp_(rp), content_(std::move(content)) {
    // nop
  }

  response_promise_mail_t(const response_promise_mail_t&) = delete;

  response_promise_mail_t& operator=(const response_promise_mail_t&) = delete;

  /// Tags the message as urgent, i.e., sends it with high priority.
  template <message_priority P = Priority,
            class E = std::enable_if_t<P == message_priority::normal>>
  [[nodiscard]] auto urgent() && {
    using result_t = response_promise_mail_t<message_priority::high, Outputs, Inputs...>;
    return result_t{rp_, std::move(content_)};
  }

  /// Satisfies the promise by delegating to another actor.
  template <class Handle>
  [[nodiscard]] auto delegate(const Handle& receiver) && {
    if constexpr (std::is_same_v<Outputs, none_t>) {
      detail::send_type_check<none_t, Handle, Inputs...>();
    } else if constexpr (detail::is_type_list_v<Outputs>) {
      // For typed response promises, check that number of args matches response types
      if constexpr (detail::tl_size_v<Outputs> == 1 &&
                    std::is_same_v<typename detail::tl_head<Outputs>::type, void>) {
        // For void response types, allow 0 arguments
        static_assert(sizeof...(Inputs) == 0, "Void response types should have no arguments");
      } else {
        static_assert(sizeof...(Inputs) == detail::tl_size_v<Outputs>,
                      "Number of arguments must match number of response types");
      }
    } else {
      detail::send_type_check<Outputs, Handle, Inputs...>();
    }
    if (!receiver) {
      rp_.deliver(make_error(sec::invalid_delegate));
      if constexpr (detail::is_type_list_v<Outputs>) {
        // For typed response promises, return delegated with response types
        return delegated<typename detail::tl_apply<Outputs, std::tuple>::type>{};
      } else {
        return delegated_response_type_t<Handle, Inputs...>{};
      }
    }
    if (rp_.pending()) {
      if constexpr (sizeof...(Inputs) == 0) {
        // Forward an empty message to the target actor.
        rp_.state_->delegate_impl(actor_cast<abstract_actor*>(receiver), make_message());
        rp_.state_.reset();
      } else {
        // Forward the message directly using the underlying delegate_impl
        if constexpr (Priority == message_priority::high)
          rp_.state_->id = rp_.state_->id.with_high_priority();
        rp_.state_->delegate_impl(actor_cast<abstract_actor*>(receiver), content_);
        rp_.state_.reset();
      }
    }
    if constexpr (detail::is_type_list_v<Outputs>) {
      // For typed response promises, return delegated with response types
      return delegated<typename detail::tl_apply<Outputs, std::tuple>::type>{};
    } else {
      return delegated_response_type_t<Handle, Inputs...>{};
    }
  }

private:
  response_promise& rp_;
  message content_;
};

namespace detail {
  // Helper to forward all but the first argument from a parameter pack
  template <class Outputs, class... Args>
  struct response_promise_mail_typed_helper;

  template <class Outputs, class First, class... Rest>
  struct response_promise_mail_typed_helper<Outputs, First, Rest...> {
    static auto make(response_promise& rp, First&&, Rest&&... rest) {
      static_assert(detail::is_type_list_v<Outputs>, "Outputs must be a type_list of response types");
      using result_t = response_promise_mail_t<message_priority::normal, Outputs, Rest...>;
      return result_t{rp, make_message_nowrap(std::forward<Rest>(rest)...)};
    }
  };
}

// Unified entry point for sending a message through a response promise.
template <class... Args>
[[nodiscard]] auto response_promise_mail(response_promise& rp, Args&&... args) {
  if constexpr (sizeof...(Args) == 0) {
    using result_t = response_promise_mail_t<message_priority::normal, none_t>;
    return result_t{rp, make_message()};
  } else if constexpr (detail::is_type_list_v<typename std::decay<typename std::tuple_element<0, std::tuple<Args...>>::type>::type>) {
    using outputs_t = typename std::decay<typename std::tuple_element<0, std::tuple<Args...>>::type>::type;
    return detail::response_promise_mail_typed_helper<outputs_t, Args...>::make(rp, std::forward<Args>(args)...);
  } else {
    using result_t = response_promise_mail_t<message_priority::normal, none_t, Args...>;
    return result_t{rp, make_message_nowrap(std::forward<Args>(args)...)};
  }
}

} // namespace caf
