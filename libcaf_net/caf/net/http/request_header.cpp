// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/http/request_header.hpp"

#include "caf/log/net.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::net::http {

namespace {

constexpr std::string_view eol = "\r\n";

/// Validate the request target part according to RFC9112.
caf::expected<uri> parse_request_target(http::method method,
                                        std::string_view request_target) {
  using namespace std::literals;
  if (request_target.empty()) {
    return make_error(sec::invalid_argument,
                      "Malformed Request-URI: request target empty.");
  }
  expected<uri> res{none};
  if (method == http::method::connect) {
    if (res = make_uri("nil://"s.append(request_target)); !res) {
      log::net::debug("Failed to parse CONNECT URI {}: {}.", request_target,
                      res.error());
      return error{sec::invalid_argument, "Malformed CONNECT Request-URI."};
    }
    if (res->authority().empty()) {
      log::net::debug("Failed to parse CONNECT URI {}: Authority missing.",
                      request_target);
      return error{sec::invalid_argument, "Malformed CONNECT Request-URI."};
    }
    return res;
  }
  if (request_target.front() == '/') {
    // The path must form a valid URI when prefixing a scheme. We don't
    // actually care about the scheme, so just use "nil" here for the
    // validation step.
    res = make_uri("nil:"s.append(request_target));
  } else if (starts_with(request_target, "http")) {
    res = make_uri(request_target);
  } else if (method == http::method::options
             && request_target == std::string_view{"*"}) {
    log::net::debug("Server-wide options request received. Converting to '/'.");
    res = make_uri("nil:/");
  }
  if (res) {
    return res;
  }
  auto msg = detail::format("Failed to parse URI {}: {}", request_target,
                            res.error());
  log::net::debug("{}", msg);
  return error{sec::invalid_argument, msg};
}

} // namespace

void request_header::clear() noexcept {
  super::clear();
  version_ = std::string_view{};
}

request_header::request_header(const request_header& other) : header(other) {
  if (other.valid()) {
    method_ = other.method_;
    uri_ = other.uri_;
    version_ = remap(other.raw_.data(), other.version_, raw_.data());
  }
}

request_header& request_header::operator=(const request_header& other) {
  super::operator=(other);
  if (other.valid()) {
    method_ = other.method_;
    uri_ = other.uri_;
    version_ = remap(other.raw_.data(), other.version_, raw_.data());
  }
  return *this;
}

std::pair<status, std::string_view>
request_header::parse(std::string_view raw) {
  auto lg = log::net::trace("raw = {}", raw);
  // Sanity checking and copying of the raw input.
  clear();
  if (raw.empty())
    return {status::bad_request, "Empty header."};
  raw_.assign(raw.begin(), raw.end());
  // Parse the first line, i.e., "METHOD REQUEST-URI VERSION".
  auto [first_line, remainder]
    = split_by(std::string_view{raw_.data(), raw_.size()}, eol);
  auto [method_str, first_line_remainder] = split_by(first_line, " ");
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
    log::net::debug("Invalid HTTP method.");
    raw_.clear();
    return {status::bad_request, "Invalid HTTP method."};
  }
  auto [uri_str, version] = split_by(first_line_remainder, " ");
  if (auto maybe_uri = parse_request_target(method_, uri_str)) {
    uri_ = std::move(*maybe_uri);
  } else {
    log::net::debug("Failed to parse URI {}: {}.", uri_str, maybe_uri.error());
    raw_.clear();
    return {status::bad_request, "Malformed Request-URI."};
  }
  // Store the remaining header fields.
  version_ = version;
  auto ok = parse_fields(remainder);
  if (ok)
    return {status::ok, "OK"};
  clear();
  return {status::bad_request, "Malformed header fields."};
}

} // namespace caf::net::http
