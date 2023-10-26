// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/response_header.hpp"

#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::net::http {

namespace {

constexpr std::string_view eol = "\r\n";

// Validate the version string.
bool validate_http_version(std::string_view str) {
  using namespace std::literals;
  auto http_prefix = "HTTP/"sv;
  if (str.find(http_prefix) != 0)
    return false;
  str.remove_prefix(http_prefix.size());
  if (str == "1" || str == "1.0" || str == "1.1")
    return true;
  // We don't support HTTP 0, 2 and 3.
  return false;
}

} // namespace

void response_header::clear() noexcept {
  super::clear();
  version_ = std::string_view{};
  status_text_ = std::string_view{};
}

response_header::response_header(const response_header& other) : super(other) {
  if (other.valid()) {
    auto base = other.raw_.data();
    auto new_base = raw_.data();
    version_ = remap(base, other.version_, new_base);
    status_ = other.status_;
    status_text_ = remap(base, other.status_text_, new_base);
  }
}

response_header& response_header::operator=(const response_header& other) {
  super::operator=(other);
  if (other.valid()) {
    auto base = other.raw_.data();
    auto new_base = raw_.data();
    version_ = remap(base, other.version_, new_base);
    status_ = other.status_;
    status_text_ = remap(base, other.status_text_, new_base);
  }
  return *this;
}

std::pair<status, std::string_view>
response_header::parse(std::string_view raw) {
  using namespace literals;
  CAF_LOG_TRACE(CAF_ARG(raw));
  // Sanity checking and copying of the raw input.
  clear();
  if (raw.empty())
    return {status::bad_request, "Empty header."};
  raw_.assign(raw.begin(), raw.end());
  // Parse the first line, i.e., "VERSION STATUS STATUS-TEXT".
  auto [first_line, remainder]
    = split_by(std::string_view{raw_.data(), raw_.size()}, eol);
  auto [version_str, first_line_remainder] = split_by(first_line, " ");
  auto [status_str, status_text] = split_by(first_line_remainder, " ");
  if (!validate_http_version(version_str)) {
    CAF_LOG_DEBUG("Invalid http version.");
    raw_.clear();
    return {status::bad_request, "Invalid HTTP version."};
  }
  version_ = version_str;
  // Parse the status from the string.
  if (auto res = get_as<uint16_t>(config_value{status_str}); !res) {
    CAF_LOG_DEBUG("Invalid status");
    raw_.clear();
    return {status::bad_request, "Invalid HTTP status."};
  } else {
    status_ = *res;
  }
  status_text_ = trim(status_text);
  if (status_text_.empty()) {
    CAF_LOG_DEBUG("Empty status text.");
    raw_.clear();
    return {status::bad_request, "Invalid HTTP status text."};
  }
  auto remaining_text = parse_fields(remainder);
  if (remaining_text)
    return {status::ok, "OK"};
  clear();
  return {status::bad_request, "Malformed header fields."};
}

} // namespace caf::net::http
