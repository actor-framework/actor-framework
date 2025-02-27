// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/web_socket/handshake.hpp"

#include "caf/net/http/lower_layer.hpp"
#include "caf/net/http/status.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/base64.hpp"
#include "caf/hash/sha1.hpp"
#include "caf/string_algorithms.hpp"

#include <algorithm>
#include <cstddef>
#include <random>
#include <tuple>

using namespace std::literals;

namespace caf::net::web_socket {

namespace {

/// Prefix for internal fields in the dictionary. Not a valid character in HTTP
/// headers.
constexpr auto internal_key_prefix = '$';

/// Internal key for storing the endpoint in the fields dictionary.
constexpr auto endpoint_key = "$endpoint"sv;

/// Internal key for storing the host in the fields dictionary.
constexpr auto host_key = "$host"sv;

/// Key for the WebSocket protocol field.
constexpr auto protocol_key = "Sec-WebSocket-Protocol"sv;

/// Key for the WebSocket origin field.
constexpr auto origin_key = "Origin"sv;

} // namespace

handshake::handshake() noexcept {
  key_.fill(std::byte{0});
}

bool handshake::has_valid_key() const noexcept {
  auto non_zero = [](std::byte x) { return x != std::byte{0}; };
  return std::any_of(key_.begin(), key_.end(), non_zero);
}

bool handshake::assign_key(std::string_view base64_key) {
  // Base 64 produces character groups of size 4. This means our key has to use
  // six groups, but the last two characters are always padding.
  if (base64_key.size() == 24 && ends_with(base64_key, "==")) {
    byte_buffer buf;
    buf.reserve(18);
    if (detail::base64::decode(base64_key, buf)) {
      CAF_ASSERT(buf.size() == 16);
      key_type bytes;
      std::copy(buf.begin(), buf.end(), bytes.begin());
      key(bytes);
      return true;
    }
  }
  return false;
}

std::string handshake::response_key() const {
  // For details on the (convoluted) algorithm see RFC 6455.
  auto str = detail::base64::encode(key_);
  str += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  auto sha = hash::sha1::compute(str);
  str.clear();
  detail::base64::encode(sha, str);
  return str;
}

void handshake::field(std::string_view key, std::string value) {
  fields_.insert_or_assign(key, std::move(value));
}

void handshake::randomize_key() {
  std::random_device rd;
  randomize_key(rd());
}

void handshake::randomize_key(unsigned seed) {
  std::minstd_rand rng{seed};
  std::uniform_int_distribution<> f{0, 255};
  for (auto& x : key_)
    x = static_cast<std::byte>(f(rng));
}

void handshake::endpoint(std::string value) {
  fields_.insert_or_assign(endpoint_key, std::move(value));
}

bool handshake::has_endpoint() const noexcept {
  return fields_.contains(endpoint_key);
}

void handshake::host(std::string value) {
  fields_.insert_or_assign(host_key, std::move(value));
}

bool handshake::has_host() const noexcept {
  return fields_.contains(host_key);
}

bool handshake::has_mandatory_fields() const noexcept {
  return has_endpoint() && has_host();
}

void handshake::origin(std::string value) {
  fields_.insert_or_assign(origin_key, std::move(value));
}

void handshake::protocols(std::string value) {
  fields_.insert_or_assign(protocol_key, std::move(value));
}

void handshake::extensions(std::string value) {
  fields_.insert_or_assign(protocol_key, std::move(value));
}

// -- HTTP generation and validation -------------------------------------------

namespace {

struct writer {
  byte_buffer* buf;
};

writer& operator<<(writer& out, std::string_view str) {
  auto bytes = as_bytes(make_span(str));
  out.buf->insert(out.buf->end(), bytes.begin(), bytes.end());
  return out;
}

template <class F>
auto operator<<(writer& out, F&& f) -> decltype(f(out)) {
  return f(out);
}

} // namespace

void handshake::write_http_1_request(byte_buffer& buf) const {
  auto encoded_key = [this](auto& out) -> decltype(auto) {
    detail::base64::encode(key_, *out.buf);
    return out;
  };
  writer out{&buf};
  out << "GET " << lookup(endpoint_key) << " HTTP/1.1\r\n"
      << "Host: " << lookup(host_key) << "\r\n"
      << "Upgrade: websocket\r\n"
      << "Connection: Upgrade\r\n"
      << "Sec-WebSocket-Version: 13\r\n"
      << "Sec-WebSocket-Key: " << encoded_key << "\r\n";
  for (auto& [key, val] : fields_) {
    if (key[0] != internal_key_prefix) {
      out << key << ": " << val << "\r\n";
    }
  }
  out << "\r\n";
}

void handshake::write_http_1_response(byte_buffer& buf) const {
  writer out{&buf};
  out << "HTTP/1.1 101 Switching Protocols\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Accept: "
      << response_key() << "\r\n\r\n";
}

void handshake::write_response(http::lower_layer::server* down) const {
  down->begin_header(http::status::switching_protocols);
  down->add_header_field("Upgrade", "websocket");
  down->add_header_field("Connection", "Upgrade");
  down->add_header_field("Sec-WebSocket-Accept", response_key());
  down->end_header();
  down->send_payload({});
}

namespace {

template <class F>
void for_each_http_line(std::string_view lines, F&& f) {
  using namespace std::literals;
  auto nl = "\r\n"sv;
  for (;;) {
    if (auto pos = lines.find(nl); pos != std::string_view::npos) {
      auto line = std::string_view{lines.data(), pos};
      if (!line.empty())
        f(std::string_view{lines.data(), pos});
      lines.remove_prefix(pos + 2);
    } else {
      return;
    }
  }
}

struct response_checker {
  std::string_view ws_key;
  bool has_status_101 = false;
  bool has_upgrade_field = false;
  bool has_connection_field = false;
  bool has_ws_accept_field = false;

  response_checker(std::string_view key) : ws_key(key) {
    // nop
  }

  bool ok() const noexcept {
    return has_status_101 && has_upgrade_field && has_connection_field
           && has_ws_accept_field;
  }

  void operator()(std::string_view line) noexcept {
    if (starts_with(line, "HTTP/1")) {
      auto code = split_by(split_by(line, " ").second, " ").first;
      has_status_101 = code == "101";
    } else {
      auto [field, value] = split_by(line, ":");
      field = trim(field);
      value = trim(value);
      if (field == "Upgrade")
        has_upgrade_field = icase_equal(value, "websocket");
      else if (field == "Connection")
        has_connection_field = icase_equal(value, "upgrade");
      else if (field == "Sec-WebSocket-Accept")
        has_ws_accept_field = value == ws_key;
    }
  }
};

} // namespace

bool handshake::is_valid_http_1_response(std::string_view http_response) const {
  auto seed = detail::base64::encode(key_);
  seed += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  auto response_key_sha = hash::sha1::compute(seed);
  auto response_key = detail::base64::encode(response_key_sha);
  response_checker checker{response_key};
  for_each_http_line(http_response, checker);
  return checker.ok();
}

// -- utility ------------------------------------------------------------------

std::string_view handshake::lookup(std::string_view field_name) const noexcept {
  if (auto i = fields_.find(field_name); i != fields_.end())
    return i->second;
  else
    return std::string_view{};
}

} // namespace caf::net::web_socket
