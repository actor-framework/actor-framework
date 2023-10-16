#include "caf/net/http/v1.hpp"

#include "caf/net/http/method.hpp"

#include "caf/uri.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

using namespace std::literals;

namespace caf::net::http::v1 {

namespace {

struct writer {
  byte_buffer* buf;
};

writer& operator<<(writer& out, char x) {
  out.buf->push_back(static_cast<std::byte>(x));
  return out;
}

writer& operator<<(writer& out, std::string_view str) {
  auto bytes = as_bytes(make_span(str));
  out.buf->insert(out.buf->end(), bytes.begin(), bytes.end());
  return out;
}

writer& operator<<(writer& out, const std::string& str) {
  return out << std::string_view{str};
}

} // namespace

std::pair<std::string_view, byte_span> split_header(const byte_span bytes) {
  constexpr std::array<std::byte, 4> end_of_header{{
    std::byte{'\r'},
    std::byte{'\n'},
    std::byte{'\r'},
    std::byte{'\n'},
  }};
  if (auto i = std::search(bytes.begin(), bytes.end(), end_of_header.begin(),
                           end_of_header.end());
      i == bytes.end()) {
    return {std::string_view{}, bytes};
  } else {
    auto offset = static_cast<size_t>(std::distance(bytes.begin(), i));
    offset += end_of_header.size();
    return {std::string_view{reinterpret_cast<const char*>(bytes.begin()),
                             offset},
            bytes.subspan(offset)};
  }
}

void write_response_header(status code, span<const string_view_pair> fields,
                           byte_buffer& buf) {
  writer out{&buf};
  out << "HTTP/1.1 "sv << std::to_string(static_cast<int>(code)) << ' '
      << phrase(code) << "\r\n"sv;
  for (auto& [key, val] : fields)
    out << key << ": "sv << val << "\r\n"sv;
  out << "\r\n"sv;
}

void begin_response_header(status code, byte_buffer& buf) {
  writer out{&buf};
  out << "HTTP/1.1 "sv << std::to_string(static_cast<int>(code)) << ' '
      << phrase(code) << "\r\n"sv;
}

void begin_request_header(http::method method, uri resource, byte_buffer& buf) {
  writer out{&buf};
  out << to_string(method) << " "sv << to_string(resource) << " HTTP/1.1\r\n"sv;
}

void add_header_field(std::string_view key, std::string_view val,
                      byte_buffer& buf) {
  writer out{&buf};
  out << key << ": "sv << val << "\r\n"sv;
}

bool end_header(byte_buffer& buf) {
  writer out{&buf};
  out << "\r\n"sv;
  return true;
}

void write_response(status code, std::string_view content_type,
                    std::string_view content, byte_buffer& buf) {
  write_response(code, content_type, content, {}, buf);
  writer out{&buf};
  out << content;
}

void write_response(status code, std::string_view content_type,
                    std::string_view content,
                    span<const string_view_pair> fields, byte_buffer& buf) {
  writer out{&buf};
  out << "HTTP/1.1 "sv << std::to_string(static_cast<int>(code)) << ' '
      << phrase(code) << "\r\n"sv;
  out << "Content-Type: "sv << content_type << "\r\n"sv;
  out << "Content-Length: "sv << std::to_string(content.size()) << "\r\n"sv;
  for (auto& [key, val] : fields)
    out << key << ": "sv << val << "\r\n"sv;
  out << "\r\n"sv;
  out << content;
}

} // namespace caf::net::http::v1
