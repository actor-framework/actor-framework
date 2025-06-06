// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/multipart_reader.hpp"

#include "caf/span.hpp"
#include "caf/string_algorithms.hpp"

using namespace std::literals;

namespace caf::net::http {

namespace {

constexpr size_t boundary_prefix_length = 9; // "boundary="

constexpr std::string_view boundary_delimiter = "--";

} // namespace

multipart_reader::multipart_reader(const responder& res)
  : res_(&res), mime_type_(res.header().field("Content-Type")) {
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
  boundary_pos += boundary_prefix_length;
  auto boundary_end = mime_type_.find(';', boundary_pos);
  auto separator = boundary_end == std::string_view::npos
                     ? mime_type_.substr(boundary_pos)
                     : mime_type_.substr(boundary_pos,
                                         boundary_end - boundary_pos);
  // Remove quotes if present
  if (separator.size() >= 2 && separator.front() == '"'
      && separator.back() == '"') {
    separator = separator.substr(1, separator.size() - 2);
  }
  if (separator.empty()) {
    return false;
  }
  auto boundary_size = separator.size() + boundary_delimiter.size();
  auto find_boundary = [separator](std::string_view payload) {
    size_t offset = 0;
    while (offset < payload.size()) {
      auto pos = payload.find(boundary_delimiter, offset);
      if (pos == std::string_view::npos) {
        return std::string_view::npos;
      }
      auto candidate = payload.substr(pos + boundary_delimiter.size());
      if (candidate.substr(0, separator.size()) == separator) {
        return pos;
      }
      offset = pos + boundary_delimiter.size();
    }
    return std::string_view::npos;
  };
  // Parse the payload into parts.
  std::string_view payload(reinterpret_cast<const char*>(res_->body().data()),
                           res_->body().size());
  // Find the first boundary
  auto pos = find_boundary(payload);
  if (pos == std::string_view::npos) {
    return false; // No valid boundary found
  }
  payload.remove_prefix(pos + boundary_size);
  for (;;) {
    // Check for final boundary
    if (caf::starts_with(payload, boundary_delimiter)) {
      break;
    }
    // Skip CRLF if present
    if (caf::starts_with(payload, "\r\n")) {
      payload.remove_prefix(2);
    }
    // Parse headers
    http::header hdr;
    auto remainder = hdr.parse_fields(payload);
    if (!remainder) {
      return false;
    }
    payload = *remainder;
    // Find the next boundary
    auto next_boundary = find_boundary(payload);
    if (next_boundary == std::string_view::npos) {
      break;
    }
    auto content_view = payload.substr(0, next_boundary);
    // Remove trailing CRLF from content if present
    if (content_view.size() >= 2
        && content_view.substr(content_view.size() - 2) == "\r\n") {
      content_view = content_view.substr(0, content_view.size() - 2);
    }
    auto content = as_bytes(make_span(content_view));
    fn(obj, std::move(hdr), content);
    payload.remove_prefix(next_boundary + boundary_size);
  }
  return true;
}

std::vector<multipart_reader::part> multipart_reader::parse(bool* ok) {
  std::vector<part> parts;
  auto res = for_each([&parts](http::header&& hdr, const_byte_span bytes) {
    parts.emplace_back(part{std::move(hdr), bytes});
  });
  if (ok != nullptr) {
    *ok = res;
  }
  return parts;
}

} // namespace caf::net::http
