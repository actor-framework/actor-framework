// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/request_header.hpp"

#include "caf/logger.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::net::http {

namespace {

constexpr std::string_view eol = "\r\n";

} // namespace

request_header::request_header(const request_header& other)
  : header_fields{other} {
  if (other.valid()) {
    method_ = other.method_;
    uri_ = other.uri_;
    version_ = remap(other.raw_.data(), other.version_, raw_.data());
  } else {
    version_ = std::string_view{};
  }
}

request_header& request_header::operator=(const request_header& other) {
  assign(other);
  return *this;
}

void request_header::assign(const request_header& other) {
  method_ = other.method_;
  uri_ = other.uri_;
  if (other.valid()) {
    raw_.assign(other.raw_.begin(), other.raw_.end());
    version_ = remap(other.raw_.data(), other.version_, raw_.data());
    reassign_fields(other);
  } else {
    raw_.clear();
    version_ = std::string_view{};
    fields_.clear();
  }
}

std::pair<status, std::string_view>
request_header::parse(std::string_view raw) {
  CAF_LOG_TRACE(CAF_ARG(raw));
  // Sanity checking and copying of the raw input.
  using namespace literals;
  if (raw.empty()) {
    raw_.clear();
    return {status::bad_request, "Empty header."};
  };
  raw_.assign(raw.begin(), raw.end());
  // Parse the first line, i.e., "METHOD REQUEST-URI VERSION".
  auto [first_line, remainder]
    = split_by(std::string_view{raw_.data(), raw_.size()}, eol);
  auto [method_str, first_line_remainder] = split_by(first_line, " ");
  auto [request_uri_str, version] = split_by(first_line_remainder, " ");
  // The path must be absolute.
  if (request_uri_str.empty() || request_uri_str.front() != '/') {
    CAF_LOG_DEBUG("Malformed Request-URI: expected an absolute path.");
    raw_.clear();
    return {status::bad_request,
            "Malformed Request-URI: expected an absolute path."};
  }
  // The path must form a valid URI when prefixing a scheme. We don't actually
  // care about the scheme, so just use "foo" here for the validation step.
  if (auto res = make_uri("nil:" + std::string{request_uri_str})) {
    uri_ = std::move(*res);
  } else {
    CAF_LOG_DEBUG("Failed to parse URI" << request_uri_str << "->"
                                        << res.error());
    raw_.clear();
    return {status::bad_request, "Malformed Request-URI."};
  }
  // Verify and store the method.
  if (icase_equal(method_str, "get")) {
    method_ = method::get;
  } else if (icase_equal(method_str, "head")) {
    method_ = method::head;
  } else if (icase_equal(method_str, "post")) {
    method_ = method::post;
  } else if (icase_equal(method_str, "put")) {
    method_ = method::put;
  } else if (icase_equal(method_str, "delete")) {
    method_ = method::del;
  } else if (icase_equal(method_str, "connect")) {
    method_ = method::connect;
  } else if (icase_equal(method_str, "options")) {
    method_ = method::options;
  } else if (icase_equal(method_str, "trace")) {
    method_ = method::trace;
  } else {
    CAF_LOG_DEBUG("Invalid HTTP method.");
    raw_.clear();
    return {status::bad_request, "Invalid HTTP method."};
  }
  // Store the remaining header fields.
  version_ = version;
  fields_.clear();
  auto ok = parse_fields(remainder);
  if (ok) {
    return {status::ok, "OK"};
  } else {
    raw_.clear();
    version_ = std::string_view{};
    fields_.clear();
    return {status::bad_request, "Malformed header fields."};
  }
}

} // namespace caf::net::http
