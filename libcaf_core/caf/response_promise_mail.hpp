// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_cast.hpp"
#include "caf/delegated.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/send_type_check.hpp"
#include "caf/error.hpp"
#include "caf/message.hpp"
#include "caf/message_priority.hpp"
#include "caf/response_promise.hpp"
#include "caf/response_type.hpp"
#include "caf/sec.hpp"
#include "caf/type_list.hpp"

#include <type_traits>
#include <utility>

namespace caf {

namespace detail {

/// Creates an empty message if no arguments are given and otherwise forwards
/// the arguments to `make_message_nowrap`.
template <class... Args>
message make_response_promise_message(Args&&... args) {
  if constexpr (sizeof...(Args) == 0)
    return make_message();
  else
    return make_message_nowrap(std::forward<Args>(args)...);
}

} // namespace detail

/// Provides a fluent interface for delegating a response promise to another
/// actor.
template <message_priority Priority, class Outputs, class... Inputs>
class [[nodiscard]] response_promise_mail_t {
public:
  response_promise_mail_t(response_promise& rp, message content)
    : rp_(rp), content_(std::move(content)) {
    // nop
  }

  response_promise_mail_t(response_promise_mail_t&&) = default;

  response_promise_mail_t(const response_promise_mail_t&) = delete;

  response_promise_mail_t& operator=(const response_promise_mail_t&) = delete;

  /// Tags the message as urgent, i.e., sends it with high priority.
  template <message_priority P = Priority>
    requires(P == message_priority::normal)
  [[nodiscard]] auto urgent() && {
    return response_promise_mail_t<message_priority::high, Outputs, Inputs...>{
      rp_, std::move(content_)};
  }

  /// Satisfies the promise by delegating to `receiver`.
  template <class Handle>
  [[nodiscard]] auto delegate(const Handle& receiver) && {
    static_assert((detail::sendable<Inputs> && ...),
                  "at least one type has no ID, "
                  "did you forgot to announce it via CAF_ADD_TYPE_ID?");
    using inputs = type_list<Inputs...>;
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
    using result_t = delegated_response_type_t<Handle, Inputs...>;
    if (!receiver) {
      rp_.deliver(make_error(sec::invalid_delegate));
      return result_t{};
    }
    if (rp_.pending()) {
      if constexpr (Priority == message_priority::high)
        rp_.state_->id = rp_.state_->id.with_high_priority();
      rp_.state_->delegate_impl(actor_cast<abstract_actor*>(receiver),
                                std::move(content_));
      rp_.state_.reset();
    }
    return result_t{};
  }

private:
  response_promise& rp_;
  message content_;
};

/// Starts a new message that transfers the responsibility for responding to
/// the original sender to another actor.
template <class Outputs = none_t, class... Args>
[[nodiscard]] auto response_promise_mail(response_promise& rp, Args&&... args) {
  using result_t
    = response_promise_mail_t<message_priority::normal, Outputs,
                              detail::strip_and_convert_t<Args>...>;
  return result_t{rp, detail::make_response_promise_message(
                        std::forward<Args>(args)...)};
}

template <class... Args>
auto response_promise::mail(Args&&... args) {
  return response_promise_mail(*this, std::forward<Args>(args)...);
}

template <message_priority P, class Handle, class... Ts>
auto response_promise::delegate(const Handle& receiver, Ts&&... args) {
  if constexpr (P == message_priority::high)
    return mail(std::forward<Ts>(args)...).urgent().delegate(receiver);
  else
    return mail(std::forward<Ts>(args)...).delegate(receiver);
}

} // namespace caf
