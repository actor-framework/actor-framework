// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/detail/type_list.hpp"
#include "caf/make_message.hpp"
#include "caf/response_promise.hpp"

namespace caf {

/// A response promise can be used to deliver a uniquely identifiable
/// response message from the server (i.e. receiver of the request)
/// to the client (i.e. the sender of the request).
template <class... Ts>
class typed_response_promise {
public:
  using forwarding_stack = response_promise::forwarding_stack;

  /// Constructs an invalid response promise.
  typed_response_promise() = default;

  typed_response_promise(none_t x) : promise_(x) {
    // nop
  }

  typed_response_promise(strong_actor_ptr self, strong_actor_ptr source,
                         forwarding_stack stages, message_id id)
    : promise_(std::move(self), std::move(source), std::move(stages), id) {
    // nop
  }

  typed_response_promise(strong_actor_ptr self, mailbox_element& src)
    : promise_(std::move(self), src) {
    // nop
  }

  typed_response_promise(typed_response_promise&&) = default;
  typed_response_promise(const typed_response_promise&) = default;
  typed_response_promise& operator=(typed_response_promise&&) = default;
  typed_response_promise& operator=(const typed_response_promise&) = default;

  /// Implicitly convertible to untyped response promise.
  [[deprecated("Use the typed_response_promise directly.")]]
  operator response_promise&() {
    return promise_;
  }

  /// Satisfies the promise by sending a non-error response message.
  template <class... Us>
  std::enable_if_t<(std::is_constructible<Ts, Us>::value && ...)>
  deliver(Us... xs) {
    promise_.deliver(make_message(Ts{std::forward<Us>(xs)}...));
  }

  /// Satisfies the promise by sending an empty response message.
  template <class L = detail::type_list<Ts...>>
  std::enable_if_t<std::is_same<L, detail::type_list<void>>::value> deliver() {
    promise_.deliver();
  }

  /// Satisfies the promise by sending an error response message.
  /// For non-requests, nothing is done.
  void deliver(error x) {
    promise_.deliver(std::move(x));
  }

  /// Satisfies the promise by sending either an error or a non-error response
  /// message.
  template <class T>
  std::enable_if_t<
    std::is_same<detail::type_list<T>, detail::type_list<Ts...>>::value>
  deliver(expected<T> x) {
    promise_.deliver(std::move(x));
  }

  /// Satisfies the promise by delegating to another actor.
  template <message_priority P = message_priority::normal, class Handle = actor,
            class... Us>
  auto delegate(const Handle& dest, Us&&... xs) {
    return promise_.template delegate<P>(dest, std::forward<Us>(xs)...);
  }

  /// Returns whether this response promise replies to an asynchronous message.
  bool async() const {
    return promise_.async();
  }

  /// Queries whether this promise is a valid promise that is not satisfied yet.
  bool pending() const {
    return promise_.pending();
  }

  /// Returns the source of the corresponding request.
  const strong_actor_ptr& source() const {
    return promise_.source();
  }

  /// Returns the remaining stages for the corresponding request.
  const forwarding_stack& stages() const {
    return promise_.stages();
  }

  /// Returns the actor that will receive the response, i.e.,
  /// `stages().front()` if `!stages().empty()` or `source()` otherwise.
  strong_actor_ptr next() const {
    return promise_.next();
  }

  /// Returns the message ID of the corresponding request.
  message_id id() const {
    return promise_.id();
  }

private:
  response_promise promise_;
};

} // namespace caf
