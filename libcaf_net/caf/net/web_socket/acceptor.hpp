// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/socket.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/callback.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/error.hpp"

#include <utility>

namespace caf::net::web_socket {

/// Accepts or rejects incoming connection requests.
/// @tparam Ts Denotes the types for the worker handshake.
template <class... Ts>
class acceptor {
public:
  acceptor(const http::request_header& hdr, socket_manager* parent)
    : hdr_(hdr), parent_(parent) {
    // nop
  }

  virtual ~acceptor() = default;

  /// Accepts the WebSocket handshake request.
  virtual void accept(Ts... xs) = 0;

  /// Sets a reason for rejecting the WebSocket handshake request.
  void reject(error reason) {
    reject_reason_ = reason;
    if (accepted_)
      accepted_ = false;
  }

  /// Returns whether the WebSocket handshake request was accepted.
  bool accepted() const noexcept {
    return accepted_;
  }

  /// Returns the reason for rejecting the WebSocket handshake request.
  error&& reject_reason() && noexcept {
    return std::move(reject_reason_);
  }

  /// Returns the reason for rejecting the WebSocket handshake request.
  const error& reject_reason() const& noexcept {
    return reject_reason_;
  }

  /// Returns the HTTP header of the WebSocket handshake request.
  const http::request_header& header() const noexcept {
    return hdr_;
  }

  /// Returns the socket that accepted the WebSocket connection.
  net::socket socket() const {
    return parent_->handle();
  }

protected:
  const http::request_header& hdr_;
  bool accepted_ = false;
  error reject_reason_;
  socket_manager* parent_;
};

/// Type trait that determines if a type is an `acceptor`.
template <class T>
struct is_acceptor : std::false_type {};

/// Specialization of `is_acceptor` for `acceptor` types.
template <class... Ts>
struct is_acceptor<acceptor<Ts...>> : std::true_type {};

/// A `constexpr bool` variable that evaluates to `true` if the given type is an
/// `acceptor`, `false` otherwise.
template <class T>
inline constexpr bool is_acceptor_v = is_acceptor<T>::value;

} // namespace caf::net::web_socket

namespace caf::detail {

template <class... Ts>
class ws_acceptor_impl : public net::web_socket::acceptor<Ts...> {
public:
  using super = net::web_socket::acceptor<Ts...>;

  using super::super;

  using frame = net::web_socket::frame;

  /// The event type for informing the application of an accepted connection.
  using app_event_type = cow_tuple<async::consumer_resource<frame>,
                                   async::producer_resource<frame>, Ts...>;

  /// The pair of resources for the WebSocket worker.
  using ws_res_type = async::resource_pair<frame, frame>;

  void accept(Ts... xs) override {
    if (super::accepted_)
      return;
    auto [app_pull, ws_push] = async::make_spsc_buffer_resource<frame>();
    auto [ws_pull, app_push] = async::make_spsc_buffer_resource<frame>();
    ws_resources = ws_res_type{ws_pull, ws_push};
    app_event = make_cow_tuple(app_pull, app_push, std::move(xs)...);
    super::accepted_ = true;
  }

  ws_res_type ws_resources;

  app_event_type app_event;
};

} // namespace caf::detail
