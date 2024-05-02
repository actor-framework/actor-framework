// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/accept_event.hpp"
#include "caf/net/http/route.hpp"
#include "caf/net/web_socket/acceptor.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/handshake.hpp"

#include "caf/async/blocking_producer.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/ws_conn_acceptor.hpp"
#include "caf/detail/ws_flow_bridge.hpp"
#include "caf/type_list.hpp"

#include <memory>

namespace caf::detail {

template <class OnRequest, class OnStart>
struct ws_switch_protocol_state : public ref_counted {
  template <class OnStartInit>
  ws_switch_protocol_state(OnRequest on_request_fn, OnStartInit&& on_start_fn)
    : on_request(std::move(on_request_fn)),
      on_start(std::forward<OnStartInit>(on_start_fn)) {
    // nop
  }

  ws_switch_protocol_state* copy() const {
    return new ws_switch_protocol_state{on_request, on_start};
  }

  OnRequest on_request;
  std::optional<OnStart> on_start;
};

template <class State, class Out, class... Ts>
class ws_switch_protocol;

template <class State, class... Out, class... Ts>
class ws_switch_protocol<State, type_list<Out...>, Ts...> {
public:
  using accept_event_t = net::accept_event<net::web_socket::frame, Out...>;

  using producer_type = async::blocking_producer<accept_event_t>;

  using shared_producer_type = std::shared_ptr<producer_type>;

  explicit ws_switch_protocol(intrusive_ptr<State> state)
    : state_(std::move(state)) {
    // nop
  }

  ws_switch_protocol(ws_switch_protocol&&) = default;

  ws_switch_protocol(const ws_switch_protocol&) = default;

  ws_switch_protocol& operator=(ws_switch_protocol&&) = default;

  ws_switch_protocol& operator=(const ws_switch_protocol&) = default;

  void operator()(net::http::responder& res, Ts... args) {
    namespace http = net::http;
    auto& hdr = res.header();
    // Sanity checking.
    if (!hdr.field_equals(ignore_case, "Connection", "upgrade")
        || !hdr.field_equals(ignore_case, "Upgrade", "websocket")) {
      res.respond(net::http::status::bad_request, "text/plain",
                  "Expected a WebSocket client handshake request.");
      return;
    }
    auto sec_key = hdr.field("Sec-WebSocket-Key");
    if (sec_key.empty()) {
      res.respond(net::http::status::bad_request, "text/plain",
                  "Mandatory field Sec-WebSocket-Key missing or invalid.");
      return;
    }
    // Call user-defined on_request callback.
    ws_acceptor_impl<Out...> acc{hdr};
    (state_->on_request)(acc, args...);
    if (!acc.accepted()) {
      if (auto& err = acc.reject_reason()) {
        auto descr = to_string(err);
        res.respond(http::status::bad_request, "text/plain", descr);
      } else {
        res.respond(http::status::bad_request, "text/plain", "Bad request.");
      }
      return;
    }
    if (!producer_->push(acc.app_event)) {
      res.respond(http::status::internal_server_error, "text/plain",
                  "Upstream channel closed.");
      return;
    }
    // Finalize the WebSocket handshake.
    net::web_socket::handshake hs;
    hs.assign_key(sec_key);
    hs.write_response(res.down());
    // Switch to the WebSocket framing protocol.
    auto& [pull, push] = acc.ws_resources;
    using net::web_socket::framing;
    auto bridge = make_ws_flow_bridge(std::move(pull), std::move(push));
    res.down()->switch_protocol(framing::make_server(std::move(bridge)));
  }

  void init() {
    // Once we call init(), the route becomes active. Before, this step, we want
    // to allow the route to copy this instance freely and have multiple
    // "potential" routes. But once the server actually starts, we detach this
    // instance from the others and it becomes a "live" object.
    auto& st = state_.unshared();
    if (auto& on_start = st.on_start; on_start) {
      auto [pull, push] = async::make_spsc_buffer_resource<accept_event_t>();
      using producer_t = async::blocking_producer<accept_event_t>;
      producer_ = std::make_shared<producer_t>(producer_t{push.try_open()});
      (*on_start)(std::move(pull));
      on_start = std::nullopt;
    }
  }

private:
  intrusive_cow_ptr<State> state_;
  shared_producer_type producer_;
};

} // namespace caf::detail

namespace caf::net::web_socket {

/// Binds a `switch_protocol` invocation to a function object for on_request.
template <class OnRequest>
struct switch_protocol_bind_2 {
public:
  switch_protocol_bind_2(OnRequest on_request)
    : on_request_(std::move(on_request)) {
    // nop
  }

  template <class OnStart>
  auto on_start(OnStart on_start) && {
    using on_request_trait = detail::get_callable_trait_t<OnRequest>;
    using on_request_args = typename on_request_trait::arg_types;
    return make(on_start, on_request_args{});
  }

private:
  template <class OnStart, class... Out, class... Ts>
  auto make(OnStart& on_start,
            type_list<net::web_socket::acceptor<Out...>&, Ts...>) {
    using namespace detail;
    using state_t = ws_switch_protocol_state<OnRequest, OnStart>;
    using impl_t = ws_switch_protocol<state_t, type_list<Out...>, Ts...>;
    auto state = make_counted<state_t>(std::move(on_request_),
                                       std::move(on_start));
    static_assert(http_route_has_init_v<impl_t>);
    return impl_t{std::move(state)};
  }

  OnRequest on_request_;
};

/// DSL entry point for creating a server.
struct switch_protocol_bind_1 {
  template <class OnRequest>
  auto on_request(OnRequest on_request) {
    return switch_protocol_bind_2<OnRequest>(std::move(on_request));
  }
};

auto switch_protocol() {
  return switch_protocol_bind_1{};
}

} // namespace caf::net::web_socket
