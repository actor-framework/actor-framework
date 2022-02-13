#include "caf/net/http/v1.hpp"

#include <array>
#include <string_view>

using namespace std::literals;

namespace caf::net::http::v1 {

namespace {

struct writer {
  byte_buffer* buf;
};

writer& operator<<(writer& out, char x) {
  out.buf->push_back(static_cast<byte>(x));
  return out;
}

writer& operator<<(writer& out, string_view str) {
  auto bytes = as_bytes(make_span(str));
  out.buf->insert(out.buf->end(), bytes.begin(), bytes.end());
  return out;
}

writer& operator<<(writer& out, std::string_view str) {
  auto bytes = as_bytes(make_span(str));
  out.buf->insert(out.buf->end(), bytes.begin(), bytes.end());
  return out;
}

writer& operator<<(writer& out, const std::string& str) {
  return out << string_view{str};
}

} // namespace

std::pair<string_view, byte_span> split_header(byte_span bytes) {
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

void write_header(status code, const header_fields_map& fields,
                  byte_buffer& buf) {
  writer out{&buf};
  out << "HTTP/1.1 "sv << std::to_string(static_cast<int>(code)) << ' '
      << phrase(code) << "\r\n"sv;
  for (auto& [key, val] : fields)
    out << key << ": "sv << val << "\r\n"sv;
  out << "\r\n"sv;
}

void write_response(status code, string_view content_type, string_view content,
                    byte_buffer& buf) {
  header_fields_map fields;
  write_response(code, content_type, content, fields, buf);
  writer out{&buf};
  out << content;
}

void write_response(status code, string_view content_type, string_view content,
                    const header_fields_map& fields, byte_buffer& buf) {
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
