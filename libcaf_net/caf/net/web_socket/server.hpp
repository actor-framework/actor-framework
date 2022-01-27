// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>

#include "caf/byte_span.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/net/connection_acceptor.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/message_flow_bridge.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/handshake.hpp"
#include "caf/net/web_socket/status.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"
#include "caf/tag/mixed_message_oriented.hpp"
#include "caf/tag/stream_oriented.hpp"

namespace caf::net::web_socket {

/// Implements the server part for the WebSocket Protocol as defined in RFC
/// 6455. Initially, the layer performs the WebSocket handshake. Once completed,
/// this layer decodes RFC 6455 frames and forwards binary and text messages to
/// `UpperLayer`.
template <class UpperLayer>
class server {
public:
  // -- member types -----------------------------------------------------------

  using input_tag = tag::stream_oriented;

  using output_tag = tag::mixed_message_oriented;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit server(Ts&&... xs) : upper_layer_(std::forward<Ts>(xs)...) {
    // > A server MUST NOT mask any frames that it sends to the client.
    // See RFC 6455, Section 5.1.
    upper_layer_.mask_outgoing_frames = false;
  }

  // -- initialization ---------------------------------------------------------

  template <class LowerLayerPtr>
  error init(socket_manager* owner, LowerLayerPtr down, const settings& cfg) {
    owner_ = owner;
    cfg_ = cfg;
    down->configure_read(receive_policy::up_to(handshake::max_http_size));
    return none;
  }

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return upper_layer_.upper_layer();
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_.upper_layer();
  }

  // -- role: upper layer ------------------------------------------------------

  template <class LowerLayerPtr>
  bool prepare_send(LowerLayerPtr down) {
    return handshake_complete_ ? upper_layer_.prepare_send(down) : true;
  }

  template <class LowerLayerPtr>
  bool done_sending(LowerLayerPtr down) {
    return handshake_complete_ ? upper_layer_.done_sending(down) : true;
  }

  template <class LowerLayerPtr>
  static void continue_reading(LowerLayerPtr down) {
    down->configure_read(receive_policy::up_to(handshake::max_http_size));
  }

  template <class LowerLayerPtr>
  void abort(LowerLayerPtr down, const error& reason) {
    if (handshake_complete_)
      upper_layer_.abort(down, reason);
  }

  template <class LowerLayerPtr>
  ptrdiff_t consume(LowerLayerPtr down, byte_span input, byte_span delta) {
    CAF_LOG_TRACE(CAF_ARG2("socket", down->handle().id)
                  << CAF_ARG2("bytes", input.size()));
    // Short circuit to the framing layer after the handshake completed.
    if (handshake_complete_)
      return upper_layer_.consume(down, input, delta);
    // Check whether received a HTTP header or else wait for more data or abort
    // when exceeding the maximum size.
    auto [hdr, remainder] = handshake::split_http_1_header(input);
    if (hdr.empty()) {
      if (input.size() >= handshake::max_http_size) {
        down->begin_output();
        handshake::write_http_1_header_too_large(down->output_buffer());
        down->end_output();
        auto err = make_error(pec::too_many_characters,
                              "exceeded maximum header size");
        down->abort_reason(std::move(err));
        return -1;
      } else {
        return 0;
      }
    } else if (!handle_header(down, hdr)) {
      return -1;
    } else if (remainder.empty()) {
      CAF_ASSERT(hdr.size() == input.size());
      return hdr.size();
    } else {
      CAF_LOG_DEBUG(CAF_ARG2("socket", down->handle().id)
                    << CAF_ARG2("remainder", remainder.size()));
      if (auto sub_result = upper_layer_.consume(down, remainder, remainder);
          sub_result >= 0) {
        return hdr.size() + sub_result;
      } else {
        return sub_result;
      }
    }
  }

  bool handshake_complete() const noexcept {
    return handshake_complete_;
  }

private:
  // -- HTTP request processing ------------------------------------------------

  template <class LowerLayerPtr>
  bool handle_header(LowerLayerPtr down, string_view http) {
    using namespace std::literals;
    // Parse the first line, i.e., "METHOD REQUEST-URI VERSION".
    auto [first_line, remainder] = split(http, "\r\n");
    auto [method, request_uri_str, version] = split2(first_line, " ");
    auto& hdr = cfg_["web-socket"].as_dictionary();
    if (method != "GET") {
      down->begin_output();
      handshake::write_http_1_bad_request(down->output_buffer(),
                                          "Expected WebSocket handshake.");
      down->end_output();
      auto err = make_error(pec::invalid_argument,
                            "invalid operation: expected GET, got "
                              + to_string(method));
      down->abort_reason(std::move(err));
      return false;
    }
    // The path must be absolute.
    if (request_uri_str.empty() || request_uri_str.front() != '/') {
      auto descr = "Malformed Request-URI path: expected absolute path."s;
      down->begin_output();
      handshake::write_http_1_bad_request(down->output_buffer(), descr);
      down->end_output();
      down->abort_reason(make_error(pec::invalid_argument, std::move(descr)));
      return false;
    }
    // The path must form a valid URI when prefixing a scheme. We don't actually
    // care about the scheme, so just use "foo" here for the validation step.
    uri request_uri;
    if (auto res = make_uri("foo://localhost" + to_string(request_uri_str))) {
      request_uri = std::move(*res);
    } else {
      auto descr = "Malformed Request-URI path: " + to_string(res.error());
      descr += '.';
      down->begin_output();
      handshake::write_http_1_bad_request(down->output_buffer(), descr);
      down->end_output();
      down->abort_reason(make_error(pec::invalid_argument, std::move(descr)));
      return false;
    }
    // Store the request information in the settings for the upper layer.
    put(hdr, "method", method);
    put(hdr, "path", request_uri.path());
    put(hdr, "query", request_uri.query());
    put(hdr, "fragment", request_uri.fragment());
    put(hdr, "http-version", version);
    // Store the remaining header fields.
    auto& fields = hdr["fields"].as_dictionary();
    for_each_line(remainder, [&fields](string_view line) {
      if (auto sep = std::find(line.begin(), line.end(), ':');
          sep != line.end()) {
        auto key = trim({line.begin(), sep});
        auto val = trim({sep + 1, line.end()});
        if (!key.empty())
          put(fields, key, val);
      }
    });
    // Check whether the mandatory fields exist.
    handshake hs;
    if (auto skey_field = get_if<std::string>(&fields, "Sec-WebSocket-Key");
        skey_field && hs.assign_key(*skey_field)) {
      CAF_LOG_DEBUG("received Sec-WebSocket-Key" << *skey_field);
    } else {
      auto descr = "Mandatory field Sec-WebSocket-Key missing or invalid."s;
      down->begin_output();
      handshake::write_http_1_bad_request(down->output_buffer(), descr);
      down->end_output();
      CAF_LOG_DEBUG("received invalid WebSocket handshake");
      down->abort_reason(make_error(pec::missing_field, std::move(descr)));
      return false;
    }
    // Try initializing the upper layer.
    if (auto err = upper_layer_.init(owner_, down, cfg_)) {
      auto descr = to_string(err);
      down->begin_output();
      handshake::write_http_1_bad_request(down->output_buffer(), descr);
      down->end_output();
      down->abort_reason(std::move(err));
      return false;
    }
    // Send server handshake.
    down->begin_output();
    hs.write_http_1_response(down->output_buffer());
    down->end_output();
    // Done.
    CAF_LOG_DEBUG("completed WebSocket handshake");
    handshake_complete_ = true;
    return true;
  }

  template <class F>
  void for_each_line(string_view input, F&& f) {
    constexpr string_view eol{"\r\n"};
    for (auto pos = input.begin();;) {
      auto line_end = std::search(pos, input.end(), eol.begin(), eol.end());
      if (line_end == input.end())
        return;
      f(string_view{pos, line_end});
      pos = line_end + eol.size();
    }
  }

  static string_view trim(string_view str) {
    str.remove_prefix(std::min(str.find_first_not_of(' '), str.size()));
    auto trim_pos = str.find_last_not_of(' ');
    if (trim_pos != str.npos)
      str.remove_suffix(str.size() - (trim_pos + 1));
    return str;
  }

  /// Splits `str` at the first occurrence of `sep` into the head and the
  /// remainder (excluding the separator).
  static std::pair<string_view, string_view> split(string_view str,
                                                   string_view sep) {
    auto i = std::search(str.begin(), str.end(), sep.begin(), sep.end());
    if (i != str.end())
      return {{str.begin(), i}, {i + sep.size(), str.end()}};
    return {{str}, {}};
  }

  /// Convenience function for splitting twice.
  static std::tuple<string_view, string_view, string_view>
  split2(string_view str, string_view sep) {
    auto [first, r1] = split(str, sep);
    auto [second, third] = split(r1, sep);
    return {first, second, third};
  }

  /// Stores whether the WebSocket handshake completed successfully.
  bool handshake_complete_ = false;

  /// Stores the upper layer.
  framing<UpperLayer> upper_layer_;

  /// Stores a pointer to the owning manager for the delayed initialization.
  socket_manager* owner_ = nullptr;

  /// Holds a copy of the settings in order to delay initialization of the upper
  /// layer until the handshake completed. We also fill this dictionary with the
  /// contents of the HTTP GET header.
  settings cfg_;
};

/// Creates a WebSocket server on the connected socket `fd`.
/// @param mpx The multiplexer that takes ownership of the socket.
/// @param fd A connected stream socket.
/// @param in Inputs for writing to the socket.
/// @param out Outputs from the socket.
/// @param trait Converts between the native and the wire format.
template <template <class> class Transport = stream_transport, class Socket,
          class T, class Trait>
socket_manager_ptr make_server(multiplexer& mpx, Socket fd,
                               async::consumer_resource<T> in,
                               async::producer_resource<T> out, Trait trait) {
  using app_t = message_flow_bridge<T, Trait, tag::mixed_message_oriented>;
  using stack_t = Transport<server<app_t>>;
  auto mgr = make_socket_manager<stack_t>(fd, &mpx, std::move(trait));
  mgr->top_layer().connect_flows(mgr.get(), std::move(in), std::move(out));
  return mgr;
}

} // namespace caf::net::web_socket

namespace caf::detail {

template <class T, class Trait>
using on_request_result = expected<
  std::tuple<async::consumer_resource<T>, // For the connection to read from.
             async::producer_resource<T>, // For the connection to write to.
             Trait>>; // For translating between native and wire format.

template <class T>
struct is_on_request_result : std::false_type {};

template <class T, class Trait>
struct is_on_request_result<on_request_result<T, Trait>> : std::true_type {};

template <class T>
struct on_request_trait;

template <class T, class ServerTrait>
struct on_request_trait<on_request_result<T, ServerTrait>> {
  using value_type = T;
  using trait_type = ServerTrait;
};

template <class OnRequest>
class ws_accept_trait {
public:
  using on_request_r
    = decltype(std::declval<OnRequest&>()(std::declval<const settings&>()));

  static_assert(is_on_request_result<on_request_r>::value,
                "OnRequest must return an on_request_result");

  using on_request_t = on_request_trait<on_request_r>;

  using value_type = typename on_request_t::value_type;

  using decorated_trait = typename on_request_t::trait_type;

  using consumer_resource_t = async::consumer_resource<value_type>;

  using producer_resource_t = async::producer_resource<value_type>;

  using in_out_tuple = std::tuple<consumer_resource_t, producer_resource_t>;

  using init_res_t = expected<in_out_tuple>;

  ws_accept_trait() = delete;

  explicit ws_accept_trait(OnRequest on_request) : state_(on_request) {
    // nop
  }

  ws_accept_trait(ws_accept_trait&&) = default;

  ws_accept_trait& operator=(ws_accept_trait&&) = default;

  init_res_t init(const settings& cfg) {
    auto f = std::move(std::get<OnRequest>(state_));
    if (auto res = f(cfg)) {
      auto& [in, out, trait] = *res;
      if (auto err = trait.init(cfg)) {
        state_ = none;
        return std::move(res.error());
      } else {
        state_ = std::move(trait);
        return std::make_tuple(std::move(in), std::move(out));
      }
    } else {
      state_ = none;
      return std::move(res.error());
    }
  }

  bool converts_to_binary(const value_type& x) {
    return decorated().converts_to_binary(x);
  }

  bool convert(const value_type& x, byte_buffer& bytes) {
    return decorated().convert(x, bytes);
  }

  bool convert(const value_type& x, std::vector<char>& text) {
    return decorated().convert(x, text);
  }

  bool convert(const_byte_span bytes, int32_t&x) {
    return decorated().convert(bytes, x);
  }

  bool convert(string_view text, int32_t& x) {
    return decorated().convert(text, x);
  }

private:
  decorated_trait& decorated() {
    return std::get<decorated_trait>(state_);
  }

  std::variant<none_t, OnRequest, decorated_trait> state_;
};

template <template <class> class Transport, class OnRequest>
class ws_acceptor_factory {
public:
  explicit ws_acceptor_factory(OnRequest on_request)
    : on_request_(std::move(on_request)) {
    // nop
  }

  error init(net::socket_manager*, const settings&) {
    return none;
  }

  template <class Socket>
  net::socket_manager_ptr make(Socket fd, net::multiplexer* mpx) {
    using trait_t = ws_accept_trait<OnRequest>;
    using value_type = typename trait_t::value_type;
    using app_t = net::message_flow_bridge<value_type, trait_t,
                                           tag::mixed_message_oriented>;
    using stack_t = Transport<net::web_socket::server<app_t>>;
    return net::make_socket_manager<stack_t>(fd, mpx, trait_t{on_request_});
  }

  void abort(const error&) {
    // nop
  }

private:
  OnRequest on_request_;
};

} // namespace caf::detail

namespace caf::net::web_socket {

/// Creates a WebSocket server on the connected socket `fd`.
/// @param mpx The multiplexer that takes ownership of the socket.
/// @param fd An accept socket in listening mode. For a TCP socket, this socket
///           must already listen to an address plus port.
/// @param on_request Function object for turning requests into a tuple
///                   consisting of a consumer resource, a producer resource,
///                   and a trait. These arguments get forwarded to
///                   @ref make_server internally.
template <template <class> class Transport = stream_transport, class Socket,
          class OnRequest>
void accept(multiplexer& mpx, Socket fd, OnRequest on_request,
            size_t limit = 0) {
  using factory = detail::ws_acceptor_factory<Transport, OnRequest>;
  using impl = connection_acceptor<Socket, factory>;
  auto ptr = make_socket_manager<impl>(std::move(fd), &mpx, limit,
                                       factory{std::move(on_request)});
  mpx.init(ptr);
}

} // namespace caf::net::web_socket
