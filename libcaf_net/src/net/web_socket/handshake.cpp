// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/handshake.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <random>
#include <tuple>




#include <iostream>

#include "caf/config.hpp"
#include "caf/detail/base64.hpp"
#include "caf/hash/sha1.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::net::web_socket {

handshake::handshake() noexcept {
  key_.fill(byte{0});
}

bool handshake::has_valid_key() const noexcept {
  auto non_zero = [](byte x) { return x != byte{0}; };
  return std::any_of(key_.begin(), key_.end(), non_zero);
}

bool handshake::assign_key(string_view base64_key) {
  // Base 64 produces character groups of size 4. This means our key has to use
  // six groups, but the last two characters are always padding.
  if (base64_key.size() == 24 && ends_with(base64_key, "==")) {
    std::vector<byte> buf;
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

void handshake::randomize_key() {
  std::random_device rd;
  randomize_key(rd());
}

void handshake::randomize_key(unsigned seed) {
  std::minstd_rand rng{seed};
  std::uniform_int_distribution<> f{0, 255};
  for (auto& x : key_)
    x = static_cast<byte>(f(rng));
}

bool handshake::has_mandatory_fields() const noexcept {
  return fields_.contains("_endpoint") && fields_.contains("_host");
}

// -- HTTP generation and validation -------------------------------------------

namespace {

struct writer {
  byte_buffer* buf;
};

writer& operator<<(writer& out, string_view str) {
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
  out << "GET " << lookup("_endpoint") << " HTTP/1.1\r\n"
      << "Host: " << lookup("_host") << "\r\n"
      << "Upgrade: websocket\r\n"
      << "Connection: Upgrade\r\n"
      << "Sec-WebSocket-Version: 13\r\n"
      << "Sec-WebSocket-Key: " << encoded_key << "\r\n";
  for (auto& [key, val] : fields_)
    if (key[0] != '_')
      out << key << ": " << val << "\r\n";
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

void handshake::write_http_1_bad_request(byte_buffer& buf, string_view descr) {
  std::cout<<"BAD REQUEST: "<<descr<<'\n';
  writer out{&buf};
  out << "HTTP/1.1 400 Bad Request\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
      << descr << "\r\n";
}

void handshake::write_http_1_header_too_large(byte_buffer& buf) {
  writer out{&buf};
  out << "HTTP/1.1 431 Request Header Fields Too Large\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
         "Header exceeds 2048 Bytes.\r\n";
}

namespace {

template <class F>
void for_each_http_line(string_view lines, F&& f) {
  using namespace caf::literals;
  auto nl = "\r\n"_sv;
  for (;;) {
    if (auto pos = lines.find(nl); pos != string_view::npos) {
      auto line = string_view{lines.data(), pos};
      if (!line.empty())
        f(string_view{lines.data(), pos});
      lines.remove_prefix(pos + 2);
    } else {
      return;
    }
  }
}

// Splits `str` at the first occurrence of `sep` into the head and the
// remainder (excluding the separator).
std::pair<string_view, string_view> split(string_view str, string_view sep) {
  auto i = std::search(str.begin(), str.end(), sep.begin(), sep.end());
  if (i != str.end())
    return {{str.begin(), i}, {i + sep.size(), str.end()}};
  return {{str}, {}};
}

// Convenience function for splitting twice.
std::tuple<string_view, string_view, string_view> split2(string_view str,
                                                         string_view sep) {
  auto [first, r1] = split(str, sep);
  auto [second, third] = split(r1, sep);
  return {first, second, third};
}

void trim(string_view& str) {
  auto non_whitespace = [](char c) { return !isspace(c); };
  if (std::any_of(str.begin(), str.end(), non_whitespace)) {
    while (str.front() == ' ')
      str.remove_prefix(1);
    while (str.back() == ' ')
      str.remove_suffix(1);
  } else {
    str = string_view{};
  }
}

bool lowercase_equal(string_view x, string_view y) {
  if (x.size() != y.size()) {
    return false;
  } else {
    for (size_t index = 0; index < x.size(); ++index)
      if (tolower(x[index]) != tolower(y[index]))
        return false;
    return true;
  }
}

struct response_checker {
  string_view ws_key;
  bool has_status_101 = false;
  bool has_upgrade_field = false;
  bool has_connection_field = false;
  bool has_ws_accept_field = false;

  response_checker(string_view key) : ws_key(key) {
    // nop
  }

  bool ok() const noexcept {
    return has_status_101 && has_upgrade_field && has_connection_field
           && has_ws_accept_field;
  }

  void operator()(string_view line) noexcept {
    if (starts_with(line, "HTTP/1")) {
      string_view code;
      std::tie(std::ignore, code, std::ignore) = split2(line, " ");
      has_status_101 = code == "101";
    } else {
      auto [field, value] = split(line, ":");
      trim(field);
      trim(value);
      if (field == "Upgrade")
        has_upgrade_field = lowercase_equal(value, "websocket");
      else if (field == "Connection")
        has_connection_field = lowercase_equal(value, "upgrade");
      else if (field == "Sec-WebSocket-Accept")
        has_ws_accept_field = value == ws_key;
    }
  }
};

} // namespace

bool handshake::is_valid_http_1_response(string_view http_response) const {
  auto seed = detail::base64::encode(key_);
  seed += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  auto response_key_sha = hash::sha1::compute(seed);
  auto response_key = detail::base64::encode(response_key_sha);
  response_checker checker{response_key};
  for_each_http_line(http_response, checker);
  return checker.ok();
}

std::pair<string_view, byte_span>
handshake::split_http_1_header(byte_span bytes) {
  std::array<byte, 4> end_of_header{{
    byte{'\r'},
    byte{'\n'},
    byte{'\r'},
    byte{'\n'},
  }};
  if (auto i = std::search(bytes.begin(), bytes.end(), end_of_header.begin(),
                           end_of_header.end());
      i == bytes.end()) {
    return {string_view{}, bytes};
  } else {
    auto offset = static_cast<size_t>(std::distance(bytes.begin(), i));
    offset += end_of_header.size();
    return {string_view{reinterpret_cast<const char*>(bytes.begin()), offset},
            bytes.subspan(offset)};
  }
}

// -- utility ------------------------------------------------------------------

string_view handshake::lookup(string_view field_name) const noexcept {
  if (auto i = fields_.find(field_name); i != fields_.end())
    return i->second;
  else
    return string_view{};
}

} // namespace caf::net::web_socket
