// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <vector>

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/check_typed_input.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/message.hpp"
#include "caf/message_id.hpp"
#include "caf/response_type.hpp"

namespace caf {

/// Enables actors to delay a response message by capturing the context of a
/// request message. This is particularly useful when an actor needs to
/// communicate to other actors in order to fulfill a request for a client.
class CAF_CORE_EXPORT response_promise {
public:
  // -- friends ----------------------------------------------------------------

  friend class local_actor;

  friend class stream_manager;

  template <class...>
  friend class typed_response_promise;

  // -- member types -----------------------------------------------------------

  using forwarding_stack = std::vector<strong_actor_ptr>;

  // -- constructors, destructors, and assignment operators --------------------

  response_promise() = default;

  response_promise(response_promise&&) = default;

  response_promise(const response_promise&) = default;

  response_promise& operator=(response_promise&&) = default;

  response_promise& operator=(const response_promise&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns whether this response promise replies to an asynchronous message.
  bool async() const noexcept;

  /// Queries whether this promise is a valid promise that is not satisfied yet.
  bool pending() const noexcept;

  /// Returns the source of the corresponding request.
  strong_actor_ptr source() const noexcept;

  /// Returns the remaining stages for the corresponding request.
  forwarding_stack stages() const;

  /// Returns the actor that will receive the response, i.e.,
  /// `stages().front()` if `!stages().empty()` or `source()` otherwise.
  strong_actor_ptr next() const noexcept;

  /// Returns the message ID of the corresponding request.
  message_id id() const noexcept;

  // -- delivery ---------------------------------------------------------------

  /// Satisfies the promise by sending the given message.
  /// @note Drops empty messages silently when responding to an asynchronous
  ///       request message, i.e., if `async() == true`.
  /// @post `pending() == false`
  void deliver(message msg);

  /// Satisfies the promise by sending an error message.
  /// @post `pending() == false`
  void deliver(error x);

  /// Satisfies the promise by sending an empty message.
  /// @note Sends no message if the request message was asynchronous, i.e., if
  ///       `async() == true`.
  /// @post `pending() == false`
  void deliver();

  /// Satisfies the promise by sending an empty message.
  /// @note Sends no message if the request message was asynchronous, i.e., if
  ///       `async() == true`.
  /// @post `pending() == false`
  void deliver(unit_t) {
    deliver();
  }

  /// Satisfies the promise by sending `make_message(xs...)`.
  /// @post `pending() == false`
  template <class... Ts>
  void deliver(Ts... xs) {
    using arg_types = detail::type_list<Ts...>;
    static_assert(!detail::tl_exists<arg_types, detail::is_result>::value,
                  "delivering a result<T> is not supported");
    static_assert(!detail::tl_exists<arg_types, detail::is_expected>::value,
                  "mixing expected<T> with regular values is not supported");
    if (pending()) {
      state_->deliver_impl(make_message(std::move(xs)...));
      state_.reset();
    }
  }

  /// Satisfies the promise by sending the content of `x`, i.e., either a value
  /// of type `T` or an @ref error.
  /// @post `pending() == false`
  template <class T>
  void deliver(expected<T> x) {
    if (pending()) {
      if (x) {
        if constexpr (std::is_same<T, void>::value
                      || std::is_same<T, unit_t>::value)
          state_->deliver_impl(make_message());
        else
          state_->deliver_impl(make_message(std::move(*x)));
      } else {
        state_->deliver_impl(make_message(std::move(x.error())));
      }
      state_.reset();
    }
  }

  // -- delegation -------------------------------------------------------------

  /// Satisfies the promise by delegating to another actor.
  /// @post `pending() == false`
  template <message_priority P = message_priority::normal, class Handle = actor,
            class... Ts>
  delegated_response_type_t<
    typename Handle::signatures,
    detail::implicit_conversions_t<typename std::decay<Ts>::type>...>
  delegate(const Handle& dest, Ts&&... xs) {
    static_assert(sizeof...(Ts) > 0, "nothing to delegate");
    using token = detail::type_list<typename detail::implicit_conversions<
      typename std::decay<Ts>::type>::type...>;
    static_assert(response_type_unbox<signatures_of_t<Handle>, token>::valid,
                  "receiver does not accept given message");
    if (pending()) {
      if constexpr (P == message_priority::high)
        state_->id = state_->id.with_high_priority();
      if constexpr (std::is_same<detail::type_list<message>,
                                 detail::type_list<std::decay_t<Ts>...>>::value)
        state_->delegate_impl(actor_cast<abstract_actor*>(dest),
                              std::forward<Ts>(xs)...);
      else
        state_->delegate_impl(actor_cast<abstract_actor*>(dest),
                              make_message(std::forward<Ts>(xs)...));
      state_.reset();
    }
    return {};
  }

private:
  // -- constructors that are visible only to friends --------------------------

  response_promise(local_actor* self, strong_actor_ptr source,
                   forwarding_stack stages, message_id id);

  response_promise(local_actor* self, mailbox_element& src);

  // -- utility functions that are visible only to friends ---------------------

  /// Sends @p response as if creating a response promise from @p self and
  /// @p request and then calling `deliver` on it but avoids extra overhead such
  /// as heap allocations for the promise.
  static void respond_to(local_actor* self, mailbox_element* request,
                         message& response);

  /// @copydoc respond_to
  static void respond_to(local_actor* self, mailbox_element* request,
                         error& response);

  // -- nested types -----------------------------------------------------------

  // Note: response promises must remain local to their owner. Hence, we don't
  //       need a thread-safe reference count for the state.
  class CAF_CORE_EXPORT state {
  public:
    state() = default;
    state(const state&) = delete;
    state& operator=(const state&) = delete;
    ~state();

    void cancel();

    void deliver_impl(message msg);

    void delegate_impl(abstract_actor* receiver, message msg);

    mutable size_t ref_count = 1;
    weak_actor_ptr weak_self;
    strong_actor_ptr source;
    forwarding_stack stages;
    message_id id;

    friend void intrusive_ptr_add_ref(const state* ptr) {
      ++ptr->ref_count;
    }

    friend void intrusive_ptr_release(const state* ptr) {
      if (--ptr->ref_count == 0)
        delete ptr;
    }
  };

  // -- member variables -------------------------------------------------------

  intrusive_ptr<state> state_;
};

} // namespace caf
