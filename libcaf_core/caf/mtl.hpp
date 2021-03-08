// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/detail/mtl_util.hpp"
#include "caf/typed_actor.hpp"

namespace caf {

/// Enables event-based actors to generate messages from a user-defined data
/// exchange format such as JSON and to send the generated messages to another
/// (typed) actor.
template <class Self, class Adapter, class Reader>
class event_based_mtl {
public:
  // -- sanity checks ----------------------------------------------------------

  static_assert(std::is_nothrow_copy_assignable<Adapter>::value);

  static_assert(std::is_nothrow_move_assignable<Adapter>::value);

  // -- constructors, destructors, and assignment operators --------------------

  event_based_mtl() = delete;

  event_based_mtl(Self* self, Adapter adapter, Reader* reader) noexcept
    : self_(self), adapter_(std::move(adapter)), reader_(reader) {
    // nop
  }

  event_based_mtl(const event_based_mtl&) noexcept = default;

  event_based_mtl& operator=(const event_based_mtl&) noexcept = default;

  // -- properties -------------------------------------------------------------

  auto self() {
    return self_;
  }

  auto& adapter() {
    return adapter_;
  }

  auto& reader() {
    return *reader_;
  }

  // -- messaging --------------------------------------------------------------

  /// Tries to get a message from the reader that matches any of the accepted
  /// inputs of `dst` and sends the converted messages on success.
  /// @param dst The destination for the next message.
  /// @returns `true` if the adapter was able to generate and send a message,
  ///          `false` otherwise.
  template <class... Fs>
  bool try_send(const typed_actor<Fs...>& dst) {
    auto dst_hdl = actor_cast<actor>(dst);
    return (detail::mtl_util<Fs>::send(self_, dst_hdl, adapter_, *reader_)
            || ...);
  }

  /// Tries to get a message from the reader that matches any of the accepted
  /// inputs of `dst` and sends a request message to `dst` on success.
  /// @param dst The destination for the next message.
  /// @param timeout The relative timeout for the request message.
  /// @param on_result The one-shot handler for the response message. This
  ///                  function object must accept *all* possible response types
  ///                  from `dst`.
  /// @param on_error The one-shot handler for timeout and other errors.
  /// @returns `true` if the adapter was able to generate and send a message,
  ///          `false` otherwise.
  template <class... Fs, class Timeout, class OnResult, class OnError>
  bool try_request(const typed_actor<Fs...>& dst, Timeout timeout,
                   OnResult on_result, OnError on_error) {
    using on_error_result = decltype(on_error(std::declval<error&>()));
    static_assert(std::is_same<void, on_error_result>::value);
    auto dst_hdl = actor_cast<actor>(dst);
    return (detail::mtl_util<Fs>::request(self_, dst_hdl, timeout, adapter_,
                                          *reader_, on_result, on_error)
            || ...);
  }

private:
  Self* self_;
  Adapter adapter_;
  Reader* reader_;
};

/// Creates an MTL (message translation layer) to enable an actor to exchange
/// messages with non-CAF endpoints over a user-defined data exchange format
/// such as JSON.
/// @param self Points to an event-based or blocking actor.
/// @param adapter Translates between internal and external message types.
/// @param reader Points to an object that either implements the interface
///               @ref deserializer directly or that provides a compatible API.
template <class Self, class Adapter, class Reader>
auto make_mtl(Self* self, Adapter adapter, Reader* reader) {
  if constexpr (std::is_base_of<non_blocking_actor_base, Self>::value) {
    return event_based_mtl{self, adapter, reader};
  } else {
    static_assert(detail::always_false_v<Self>,
                  "sorry, support for blocking actors not implemented yet");
  }
}

} // namespace caf
