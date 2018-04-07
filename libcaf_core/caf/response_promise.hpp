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

#include <vector>

#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/actor_addr.hpp"
#include "caf/message_id.hpp"
#include "caf/response_type.hpp"
#include "caf/check_typed_input.hpp"

namespace caf {

/// A response promise can be used to deliver a uniquely identifiable
/// response message from the server (i.e. receiver of the request)
/// to the client (i.e. the sender of the request).
class response_promise {
public:
  using forwarding_stack = std::vector<strong_actor_ptr>;

  /// Constructs an invalid response promise.
  response_promise();

  response_promise(none_t);

  response_promise(strong_actor_ptr self, strong_actor_ptr source,
                   forwarding_stack stages, message_id id);

  response_promise(strong_actor_ptr self, mailbox_element& src);

  response_promise(response_promise&&) = default;
  response_promise(const response_promise&) = default;
  response_promise& operator=(response_promise&&) = default;
  response_promise& operator=(const response_promise&) = default;

  /// Satisfies the promise by sending a non-error response message.
  template <class T, class... Ts>
  detail::enable_if_t<((sizeof...(Ts) > 0)
                       || (!std::is_convertible<T, error>::value
                           && !std::is_same<detail::decay_t<T>, unit_t>::value))
                        && !detail::is_expected<detail::decay_t<T>>::value>
  deliver(T&& x, Ts&&... xs) {
    using ts = detail::type_list<detail::decay_t<T>, detail::decay_t<Ts>...>;
    static_assert(!detail::tl_exists<ts, detail::is_result>::value,
                  "it is not possible to deliver objects of type result<T>");
    static_assert(!detail::tl_exists<ts, detail::is_expected>::value,
                  "mixing expected<T> with regular values is not supported");
    return deliver_impl(make_message(std::forward<T>(x),
                                     std::forward<Ts>(xs)...));
  }

  template <class T>
  void deliver(expected<T> x) {
    if (x)
      return deliver(std::move(*x));
    return deliver(std::move(x.error()));
  }

  /// Satisfies the promise by delegating to another actor.
  template <message_priority P = message_priority::normal,
           class Handle = actor, class... Ts>
   typename response_type<
    typename Handle::signatures,
    detail::implicit_conversions_t<typename std::decay<Ts>::type>...
  >::delegated_type
  delegate(const Handle& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "nothing to delegate");
    using token =
      detail::type_list<
        typename detail::implicit_conversions<
          typename std::decay<Ts>::type
        >::type...>;
    static_assert(response_type_unbox<signatures_of_t<Handle>, token>::valid,
                  "receiver does not accept given message");
    if (dest) {
      auto mid = P == message_priority::high ? id_.with_high_priority() : id_;
      dest->enqueue(make_mailbox_element(std::move(source_), mid,
                                         std::move(stages_),
                                         std::forward<Ts>(xs)...),
                    context());
    }
    return {};
  }

  /// Satisfies the promise by sending an error response message.
  void deliver(error x);

  /// Satisfies the promise by sending an empty message if this promise has a
  /// valid message ID, i.e., `async() == false`.
  void deliver(unit_t x);

  /// Returns whether this response promise replies to an asynchronous message.
  bool async() const;

  /// Queries whether this promise is a valid promise that is not satisfied yet.
  inline bool pending() const {
    return source_ != nullptr || !stages_.empty();
  }

  /// Returns the source of the corresponding request.
  inline const strong_actor_ptr& source() const {
    return source_;
  }

  /// Returns the remaining stages for the corresponding request.
  inline const forwarding_stack& stages() const {
    return stages_;
  }

  /// Returns the actor that will receive the response, i.e.,
  /// `stages().front()` if `!stages().empty()` or `source()` otherwise.
  inline strong_actor_ptr next() const {
    return stages_.empty() ? source_ : stages_.front();
  }

  /// Returns the message ID of the corresponding request.
  inline message_id id() const {
    return id_;
  }

private:
  execution_unit* context();

  void deliver_impl(message msg);

  strong_actor_ptr self_;
  strong_actor_ptr source_;
  forwarding_stack stages_;
  message_id id_;
};

} // namespace caf

