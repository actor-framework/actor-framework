// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>

#include "caf/byte_span.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/handshake.hpp"
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
  explicit server(Ts&&... xs)
    : upper_layer_(std::forward<Ts>(xs)...) {
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
    // Parse the first line, i.e., "METHOD REQUEST-URI VERSION".
    auto [first_line, remainder] = split(http, "\r\n");
    auto [method, request_uri, version] = split2(first_line, " ");
    auto& hdr = cfg_["web-socket"].as_dictionary();
    if (method != "GET") {
      auto err = make_error(pec::invalid_argument,
                            "invalid operation: expected GET, got "
                              + to_string(method));
      down->abort_reason(std::move(err));
      return false;
    }
    // Store the request information in the settings for the upper layer.
    put(hdr, "method", method);
    put(hdr, "request-uri", request_uri);
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
      CAF_LOG_DEBUG("received invalid WebSocket handshake");
      down->abort_reason(
        make_error(pec::missing_field,
                   "mandatory field Sec-WebSocket-Key missing or invalid"));
      return false;
    }
    // Send server handshake.
    down->begin_output();
    hs.write_http_1_response(down->output_buffer());
    down->end_output();
    // Try initializing the upper layer.
    if (auto err = upper_layer_.init(owner_, down, cfg_)) {
      down->abort_reason(std::move(err));
      return false;
    }
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
  /// layer until the handshake completed.
  settings cfg_;
};

} // namespace caf::net::web_socket
