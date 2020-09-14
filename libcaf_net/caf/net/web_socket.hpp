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

#include "caf/detail/move_if_not_ptr.hpp"
#include "caf/error.hpp"
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

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  web_socket(Ts&&... xs) : upper_layer_(std::forward<Ts>(xs)...) {
    // nop
  }

  virtual ~web_socket() {
    // nop
  }

  // -- constants --------------------------------------------------------------

  static constexpr std::array<byte, 4> end_of_header{{
    byte{'\r'},
    byte{'\n'},
    byte{'\r'},
    byte{'\n'},
  }};

  // -- properties -------------------------------------------------------------

  auto& upper_layer() noexcept {
    return upper_layer_;
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_;
  }

  // -- initialization ---------------------------------------------------------

  template <class Parent>
  error init(Parent&, const settings& config) {
    cfg_ = config;
    return none;
  }

  // -- role: upper layer ------------------------------------------------------

  template <class LowerLayer>
  bool prepare_send(LowerLayer& down) {
    if (handshake_complete_)
      return upper_layer_.prepare_send(down);
    // TODO: implement me.
    return false;
  }

  template <class LowerLayer>
  bool done_sending(LowerLayer& down) {
    if (handshake_complete_)
      return upper_layer_.done_sending(down);
    // TODO: implement me.
    return false;
  }

  template <class LowerLayer>
  void abort(LowerLayer& down, const error& reason) {
    if (handshake_complete_)
      return upper_layer_.abort(down, reason);
  }

  template <class LowerLayer>
  ptrdiff_t consume(LowerLayer& down, byte_span buffer, byte_span delta) {
    if (handshake_complete_)
      return upper_layer_.consume(down, buffer, delta);
    // TODO: we could avoid repeated scans by using the delta parameter.
    auto i = std::search(buffer.begin(), buffer.end(),
                             end_of_header.begin(), end_of_header.end());
    if (i == buffer.end())
      return 0;
    auto offset = static_cast<size_t>(std::distance(buffer.begin(), i));
    offset += end_of_header.size();
    // Take all but the last two bytes (to avoid an empty line) as input for
    // the header.
    string_view header{reinterpret_cast<const char*>(buffer.begin()),
                       offset - 2};
    if (!handle_header(down, header))
      return -1;
    return offset + upper_layer_.consume(down, buffer.subspan(offset), {});
  }

private:
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
      sec_key = detail::move_if_not_ptr(skey_field);
    } else {
      auto err = make_error(pec::missing_field,
                            "Mandatory field Sec-WebSocket-Key not found");
      down.abort_reason(std::move(err));
      return false;
    }
    // Try initializing the upper layer.
    if (auto err = upper_layer_.init(down, cfg_)) {
      down.abort_reason(std::move(err));
      return false;
    }
    // Send server handshake.
    // Done.
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
    str.remove_suffix(str.size()
                      - std::min(str.find_last_not_of(' '), str.size()));
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

  /// Holds a copy of the settings in order to delay initialization of the upper
  /// layer until the handshake completed.
  settings cfg_;
};

} // namespace caf::net
