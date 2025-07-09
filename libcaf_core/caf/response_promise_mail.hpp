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

namespace caf {

/// Provides a fluent interface for sending messages through a response promise.
template <message_priority Priority, class... Args>
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
    using result_t = response_promise_mail_t<message_priority::high, Args...>;
    return result_t{rp_, std::move(content_)};
  }

  /// Satisfies the promise by delegating to another actor.
  template <class Handle>
  [[nodiscard]] auto delegate(const Handle& receiver) && {
    detail::send_type_check<none_t, Handle, Args...>();
    if (!receiver) {
      rp_.deliver(make_error(sec::invalid_delegate));
      return delegated<message>{};
    }
    if (rp_.pending()) {
      if constexpr (sizeof...(Args) == 0) {
        // Forward an empty message to the target actor.
        rp_.state_->delegate_impl(actor_cast<abstract_actor*>(receiver), make_message());
        rp_.state_.reset();
      } else {
        if constexpr (Priority == message_priority::high)
          rp_.template delegate<message_priority::high>(receiver, content_);
        else
          rp_.template delegate<message_priority::normal>(receiver, content_);
      }
    }
    return delegated<message>{};
  }

private:
  response_promise& rp_;
  message content_;
};

/// Entry point for sending a message through a response promise.
template <class... Args>
[[nodiscard]] auto response_promise_mail(response_promise& rp, Args&&... args) {
  using result_t = response_promise_mail_t<message_priority::normal, Args...>;
  return result_t{rp, make_message_nowrap(std::forward<Args>(args)...)};
}

} // namespace caf
