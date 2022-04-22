// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/net/connection_acceptor.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/http/header.hpp"
#include "caf/net/http/method.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/http/v1.hpp"
#include "caf/net/message_flow_bridge.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/handshake.hpp"
#include "caf/net/web_socket/status.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/tag/mixed_message_oriented.hpp"
#include "caf/tag/stream_oriented.hpp"

#include <algorithm>

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
    using namespace std::literals;
    CAF_LOG_TRACE(CAF_ARG2("socket", down->handle().id)
                  << CAF_ARG2("bytes", input.size()));
    // Short circuit to the framing layer after the handshake completed.
    if (handshake_complete_)
      return upper_layer_.consume(down, input, delta);
    // Check whether we received an HTTP header or else wait for more data.
    // Abort when exceeding the maximum size.
    auto [hdr, remainder] = http::v1::split_header(input);
    if (hdr.empty()) {
      if (input.size() >= handshake::max_http_size) {
        down->begin_output();
        http::v1::write_response(http::status::request_header_fields_too_large,
                                 "text/plain"sv,
                                 "Header exceeds maximum size."sv,
                                 down->output_buffer());
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
  void
  write_response(LowerLayerPtr down, http::status code, std::string_view msg) {
    down->begin_output();
    http::v1::write_response(code, "text/plain", msg, down->output_buffer());
    down->end_output();
  }

  template <class LowerLayerPtr>
  bool handle_header(LowerLayerPtr down, std::string_view http) {
    using namespace std::literals;
    // Parse the header and reject invalid inputs.
    http::header hdr;
    auto [code, msg] = hdr.parse(http);
    if (code != http::status::ok) {
      write_response(down, code, msg);
      down->abort_reason(make_error(pec::invalid_argument, "malformed header"));
      return false;
    }
    if (hdr.method() != http::method::get) {
      write_response(down, http::status::bad_request,
                     "Expected a WebSocket handshake.");
      auto err = make_error(pec::invalid_argument,
                            "invalid operation: expected method get, got "
                              + to_string(hdr.method()));
      down->abort_reason(std::move(err));
      return false;
    }
    // Check whether the mandatory fields exist.
    auto sec_key = hdr.field("Sec-WebSocket-Key");
    if (sec_key.empty()) {
      auto descr = "Mandatory field Sec-WebSocket-Key missing or invalid."s;
      write_response(down, http::status::bad_request, descr);
      CAF_LOG_DEBUG("received invalid WebSocket handshake");
      down->abort_reason(make_error(pec::missing_field, std::move(descr)));
      return false;
    }
    // Store the request information in the settings for the upper layer.
    auto& ws = cfg_["web-socket"].as_dictionary();
    put(ws, "method", to_rfc_string(hdr.method()));
    put(ws, "path", std::string{hdr.path()});
    put(ws, "query", hdr.query());
    put(ws, "fragment", hdr.fragment());
    put(ws, "http-version", hdr.version());
    if (!hdr.fields().empty()) {
      auto& fields = ws["fields"].as_dictionary();
      for (auto& [key, val] : hdr.fields())
        put(fields, std::string{key}, std::string{val});
    }
    // Try to initialize the upper layer.
    if (auto err = upper_layer_.init(owner_, down, cfg_)) {
      auto descr = to_string(err);
      CAF_LOG_DEBUG("upper layer rejected a WebSocket connection:" << descr);
      write_response(down, http::status::bad_request, descr);
      down->abort_reason(std::move(err));
      return false;
    }
    // Finalize the WebSocket handshake.
    handshake hs;
    hs.assign_key(sec_key);
    down->begin_output();
    hs.write_http_1_response(down->output_buffer());
    down->end_output();
    CAF_LOG_DEBUG("completed WebSocket handshake");
    handshake_complete_ = true;
    return true;
  }

  template <class F>
  void for_each_line(std::string_view input, F&& f) {
    constexpr std::string_view eol{"\r\n"};
    for (auto pos = input.begin();;) {
      auto line_end = std::search(pos, input.end(), eol.begin(), eol.end());
      if (line_end == input.end())
        return;
      f(make_string_view(pos, line_end));
      pos = line_end + eol.size();
    }
  }

  static std::string_view trim(std::string_view str) {
    str.remove_prefix(std::min(str.find_first_not_of(' '), str.size()));
    auto trim_pos = str.find_last_not_of(' ');
    if (trim_pos != str.npos)
      str.remove_suffix(str.size() - (trim_pos + 1));
    return str;
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
  auto mgr = make_socket_manager<stack_t>(fd, &mpx, std::move(in),
                                          std::move(out), std::move(trait));
  return mgr;
}

} // namespace caf::net::web_socket
