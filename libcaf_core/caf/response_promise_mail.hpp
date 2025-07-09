// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_cast.hpp"
#include "caf/delegated.hpp"
#include "caf/detail/send_type_check.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/message.hpp"
#include "caf/message_priority.hpp"
#include "caf/response_promise.hpp"
#include "caf/response_type.hpp"

#include <type_traits>

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
    using result_t
      = response_promise_mail_t<message_priority::high, Outputs, Inputs...>;
    return result_t{rp_, std::move(content_)};
  }

  /// Satisfies the promise by delegating to another actor.
  template <class Handle>
  void delegate(const Handle& receiver) && {
    static_assert((detail::sendable<Inputs> && ...),
                  "at least one type has no ID, "
                  "did you forgot to announce it via CAF_ADD_TYPE_ID?");
    using inputs = type_list<detail::strip_and_convert_t<Inputs>...>;
    using response_opt
      = response_type_unbox<detail::signatures_of_t<Handle>, inputs>;
    using receiver_interface = detail::signatures_of_t<Handle>;
    static_assert(response_opt::valid,
                  "receiver does not accept given message");
    if constexpr (!std::is_same_v<receiver_interface, none_t>
                  && !std::is_same_v<Outputs, none_t>) {
      using response = typename response_opt::type;
      static_assert(std::is_same_v<Outputs, response>,
                    "response type mismatch");
    }
    if (!receiver) {
      rp_.deliver(make_error(sec::invalid_delegate));
      return;
    }
    if (rp_.pending()) {
      // Forward the message directly using the underlying delegate_impl
      if constexpr (Priority == message_priority::high) {
        rp_.state_->id = rp_.state_->id.with_high_priority();
      }
      rp_.state_->delegate_impl(actor_cast<abstract_actor*>(receiver),
                                content_);
      rp_.state_.reset();
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
    static_assert(detail::is_type_list_v<Outputs>,
                  "Outputs must be a type_list of response types");
    using result_t
      = response_promise_mail_t<message_priority::normal, Outputs, Rest...>;
    return result_t{rp, make_message_nowrap(std::forward<Rest>(rest)...)};
  }
};
} // namespace detail

// Unified entry point for sending a message through a response promise.
template <class... Args>
[[nodiscard]] auto response_promise_mail(response_promise& rp, Args&&... args) {
  if constexpr (sizeof...(Args) == 0) {
    using result_t = response_promise_mail_t<message_priority::normal, none_t>;
    return result_t{rp, make_message()};
  } else if constexpr (detail::is_type_list_v<
                         typename std::decay<typename std::tuple_element<
                           0, std::tuple<Args...>>::type>::type>) {
    using outputs_t = typename std::decay<
      typename std::tuple_element<0, std::tuple<Args...>>::type>::type;
    return detail::response_promise_mail_typed_helper<outputs_t, Args...>::make(
      rp, std::forward<Args>(args)...);
  } else {
    using result_t
      = response_promise_mail_t<message_priority::normal, none_t, Args...>;
    return result_t{rp, make_message_nowrap(std::forward<Args>(args)...)};
  }
}

} // namespace caf
