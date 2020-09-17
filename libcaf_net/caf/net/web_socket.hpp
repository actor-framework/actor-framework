/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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

#include <algorithm>

#include "caf/detail/encode_base64.hpp"
#include "caf/error.hpp"
#include "caf/hash/sha1.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/pec.hpp"
#include "caf/settings.hpp"
#include "caf/tag/stream_oriented.hpp"

namespace caf::net {

/// Implements the WebSocket Protocol as defined in RFC 6455. Initially, the
/// layer performs the WebSocket handshake. Once completed, this layer becomes
/// fully transparent and forwards any data to the `UpperLayer`.
template <class UpperLayer>
class web_socket {
public:
  // -- member types -----------------------------------------------------------

  using byte_span = span<const byte>;

  using input_tag = tag::stream_oriented;

  using output_tag = tag::stream_oriented;

  // -- constants --------------------------------------------------------------

  static constexpr std::array<byte, 4> end_of_header{{
    byte{'\r'},
    byte{'\n'},
    byte{'\r'},
    byte{'\n'},
  }};

  // A handshake should usually fit into 200-300 Bytes. 2KB is more than enough.
  static constexpr uint32_t max_header_size = 2048;

  static constexpr string_view header_too_large
    = "HTTP/1.1 431 Request Header Fields Too Large\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "Header exceeds 2048 Bytes.\r\n";

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  web_socket(Ts&&... xs) : upper_layer_(std::forward<Ts>(xs)...) {
    // nop
  }

  virtual ~web_socket() {
    // nop
  }

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return upper_layer_;
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_;
  }

  // -- initialization ---------------------------------------------------------

  template <class LowerLayer>
  error init(socket_manager* owner, LowerLayer& down, const settings& config) {
    owner_ = owner;
    cfg_ = config;
    down.configure_read(net::receive_policy::up_to(max_header_size));
    return none;
  }

  // -- role: upper layer ------------------------------------------------------

  template <class LowerLayer>
  bool prepare_send(LowerLayer& down) {
    return handshake_complete_ ? upper_layer_.prepare_send(down) : true;
  }

  template <class LowerLayer>
  bool done_sending(LowerLayer& down) {
    return handshake_complete_ ? upper_layer_.done_sending(down) : true;
  }

  template <class LowerLayer>
  void abort(LowerLayer& down, const error& reason) {
    if (handshake_complete_)
      upper_layer_.abort(down, reason);
  }

  template <class LowerLayer>
  ptrdiff_t consume(LowerLayer& down, byte_span buffer, byte_span delta) {
    if (handshake_complete_)
      return upper_layer_.consume(down, buffer, delta);
    // TODO: we could avoid repeated scans by using the delta parameter.
    auto i = std::search(buffer.begin(), buffer.end(), end_of_header.begin(),
                         end_of_header.end());
    if (i == buffer.end()) {
      if (buffer.size() == max_header_size) {
        write(down, header_too_large);
        auto err = make_error(pec::too_many_characters,
                              "exceeded maximum header size");
        down.abort_reason(std::move(err));
        return -1;
      }
      return 0;
    }
    auto offset = static_cast<size_t>(std::distance(buffer.begin(), i));
    offset += end_of_header.size();
    // Take all but the last two bytes (to avoid an empty line) as input for
    // the header.
    string_view header{reinterpret_cast<const char*>(buffer.begin()),
                       offset - 2};
    if (!handle_header(down, header))
      return -1;
    ptrdiff_t sub_result = 0;
    if (offset < buffer.size()) {
      sub_result = upper_layer_.consume(down, buffer.subspan(offset), {});
      if (sub_result < 0)
        return sub_result;
    }
    return static_cast<ptrdiff_t>(offset) + sub_result;
  }

  bool handshake_complete() const noexcept {
    return handshake_complete_;
  }

private:
  template <class LowerLayer>
  static void write(LowerLayer& down, string_view output) {
    auto out = as_bytes(make_span(output));
    down.begin_output();
    auto& buf = down.output_buffer();
    buf.insert(buf.end(), out.begin(), out.end());
    down.end_output();
  }

  template <class LowerLayer>
  bool handle_header(LowerLayer& down, string_view input) {
    // Parse the first line, i.e., "METHOD REQUEST-URI VERSION".
    auto [first_line, remainder] = split(input, "\r\n");
    auto [method, request_uri, version] = split2(first_line, " ");
    auto& hdr = cfg_["web-socket"].as_dictionary();
    if (method != "GET") {
      auto err = make_error(pec::invalid_argument,
                            "invalid operation: expected GET, got "
                              + to_string(method));
      down.abort_reason(std::move(err));
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
    std::string sec_key;
    if (auto skey_field = get_if<std::string>(&fields, "Sec-WebSocket-Key")) {
      auto field_hash = hash::sha1::compute(*skey_field);
      sec_key = detail::encode_base64(field_hash);
    } else {
      auto err = make_error(pec::missing_field,
                            "Mandatory field Sec-WebSocket-Key not found");
      down.abort_reason(std::move(err));
      return false;
    }
    // Try initializing the upper layer.
    if (auto err = upper_layer_.init(owner_, down, cfg_)) {
      down.abort_reason(std::move(err));
      return false;
    }
    // Send server handshake.
    down.begin_output();
    auto& buf = down.output_buffer();
    auto append = [&buf](string_view output) {
      auto out = as_bytes(make_span(output));
      buf.insert(buf.end(), out.begin(), out.end());
    };
    append("HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: ");
    append(sec_key);
    append("\r\n\r\n");
    down.end_output();
    // Done.
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
  UpperLayer upper_layer_;

  /// Stores a pointer to the owning manager for the delayed initialization.
  socket_manager* owner_ = nullptr;

  /// Holds a copy of the settings in order to delay initialization of the upper
  /// layer until the handshake completed.
  settings cfg_;
};

} // namespace caf::net
