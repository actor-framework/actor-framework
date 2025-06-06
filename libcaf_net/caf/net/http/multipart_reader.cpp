// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/multipart_reader.hpp"

#include "caf/span.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::net::http {

multipart_reader::multipart_reader(const responder& res)
  : res_(res), mime_type_(res.header().field("Content-Type")) {
  // nop
}

bool multipart_reader::do_parse(consume_fn fn, void* obj) {
  // Ensure the MIME type indicates multipart content.
  if (mime_type_.find("multipart/") != 0) {
    return false;
  }
  // Extract the boundary from the MIME type.
  auto boundary_pos = mime_type_.find("boundary=");
  if (boundary_pos == std::string::npos) {
    return false;
  }
  std::string boundary = "--"
                         + std::string{mime_type_.substr(boundary_pos + 9)};
  std::string final_boundary = boundary + "--";
  // Parse the payload into parts.
  std::string_view payload(reinterpret_cast<const char*>(res_.body().data()),
                           res_.body().size());
  // Find the first boundary
  auto pos = payload.find(boundary);
  if (pos == std::string_view::npos) {
    return true; // no parts found
  }
  payload.remove_prefix(pos + boundary.size());
  while (true) {
    // Check for final boundary
    if (caf::starts_with(payload, "--")) {
      break;
    }
    // Skip CRLF if present
    if (caf::starts_with(payload, "\r\n")) {
      payload.remove_prefix(2);
    }
    // Parse headers
    part current_part;
    auto header_end = payload.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
      break;
    }
    current_part.headers = payload.substr(0, header_end + 2);
    payload.remove_prefix(header_end + 4);
    // Find the next boundary
    auto next_boundary = payload.find(boundary);
    if (next_boundary == std::string_view::npos) {
      break;
    }
    auto content_view = payload.substr(0, next_boundary);
    // Remove trailing CRLF from content if present
    if (content_view.size() >= 2
        && content_view.substr(content_view.size() - 2) == "\r\n") {
      content_view = content_view.substr(0, content_view.size() - 2);
    }
    current_part.content = as_bytes(make_span(content_view));
    fn(obj, current_part.headers, current_part.content);
    payload.remove_prefix(next_boundary + boundary.size());
  }
  return true;
}

std::optional<std::vector<multipart_reader::part>> multipart_reader::parse() {
  std::vector<part> parts;
  if (!for_each([&parts](std::string_view headers, const_byte_span content) {
        parts.emplace_back(part{headers, content});
      })) {
    return std::nullopt;
  }
  return parts;
}

} // namespace caf::net::http
