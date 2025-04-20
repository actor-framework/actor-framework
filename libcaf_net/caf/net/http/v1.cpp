#include "caf/net/http/v1.hpp"

#include "caf/net/http/method.hpp"

#include "caf/detail/parser/ascii_to_int.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/string_algorithms.hpp"

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

std::pair<std::string_view, byte_span> split_header(byte_span bytes) {
  constexpr auto end_of_header = std::array<std::byte, 4>{{
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

expected<std::pair<size_t, byte_span>> parse_chunk(byte_span input) {
  constexpr auto crlf = "\r\n"sv;
  auto [chunk, remainder] = split_by(to_string_view(input), crlf);
  if (chunk.size() == input.size()) {
    // Prevents indefinite octet stream as chunk_size.
    if (input.size() >= 10)
      return make_error(sec::protocol_error, "Chunk size part is too long.");
    // Didn't receive enough data. Signal to caller by returning empty error.
    return error{};
  }
  // Extensions are not supported. Look for extension separator ;
  if (std::find(chunk.begin(), chunk.end(), ';') != chunk.end())
    return make_error(sec::logic_error, "Chunk extensions not supported.");
  if (!std::all_of(chunk.begin(), chunk.end(), isxdigit))
    return make_error(sec::protocol_error, "Chunk size decoding error.");
  if (chunk.size() > sizeof(size_t))
    return make_error(sec::protocol_error,
                      "Integer overflow while parsing chunk size.");
  // This parsing method is only safe because of the previous checks.
  detail::parser::ascii_to_int<16, size_t> f;
  size_t chunk_size = 0;
  for (auto c : chunk)
    chunk_size = chunk_size * 16 + f(c);
  const auto parsed_len = input.size() - remainder.size();
  return std::make_pair(chunk_size, input.subspan(parsed_len));
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

void begin_request_header(http::method method, std::string_view path,
                          byte_buffer& buf) {
  writer out{&buf};
  out << to_rfc_string(method) << ' ' << path << " HTTP/1.1\r\n"sv;
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
