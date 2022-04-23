#include "caf/net/http/header.hpp"

#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::net::http {

namespace {

constexpr std::string_view eol = "\r\n";

template <class F>
bool for_each_line(std::string_view input, F&& f) {
  auto input_end = input.data() + input.size();
  for (auto pos = input.data();;) {
    auto line_end = std::search(pos, input_end, eol.begin(), eol.end());
    if (line_end == input_end || std::distance(pos, input_end) == 2) {
      // Reaching the end or hitting the last empty line tells us we're done.
      return true;
    }
    auto to_line_end = std::distance(pos, line_end);
    CAF_ASSERT(to_line_end >= 0);
    if (!f(std::string_view{pos, static_cast<size_t>(to_line_end)}))
      return false;
    pos = line_end + eol.size();
  }
  return true;
}

} // namespace

header::header(const header& other) {
  assign(other);
}

header& header::operator=(const header& other) {
  assign(other);
  return *this;
}

void header::assign(const header& other) {
  auto remap = [](const char* base, std::string_view src,
                  const char* new_base) {
    auto offset = std::distance(base, src.data());
    return std::string_view{new_base + offset, src.size()};
  };
  method_ = other.method_;
  uri_ = other.uri_;
  if (other.valid()) {
    raw_.assign(other.raw_.begin(), other.raw_.end());
    auto base = other.raw_.data();
    auto new_base = raw_.data();
    version_ = remap(base, other.version_, new_base);
    auto& fields = fields_.container();
    auto& other_fields = other.fields_.container();
    fields.resize(other_fields.size());
    for (size_t index = 0; index < fields.size(); ++index) {
      fields[index].first = remap(base, other_fields[index].first, new_base);
      fields[index].second = remap(base, other_fields[index].second, new_base);
    }
  } else {
    raw_.clear();
    version_ = std::string_view{};
    fields_.clear();
  }
}

std::pair<status, std::string_view> header::parse(std::string_view raw) {
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
  bool ok = for_each_line(remainder, [this](std::string_view line) {
    if (auto sep = std::find(line.begin(), line.end(), ':');
        sep != line.end()) {
      auto n = static_cast<size_t>(std::distance(line.begin(), sep));
      auto key = trim(std::string_view{line.data(), n});
      auto m = static_cast<size_t>(std::distance(sep + 1, line.end()));
      auto val = trim(std::string_view{std::addressof(*(sep + 1)), m});
      if (!key.empty()) {
        fields_.emplace(key, val);
        return true;
      }
    }
    return false;
  });
  if (ok) {
    return {status::ok, "OK"};
  } else {
    raw_.clear();
    version_ = std::string_view{};
    fields_.clear();
    return {status::bad_request, "Malformed header fields."};
  }
}

bool header::chunked_transfer_encoding() const {
  return field("Transfer-Encoding").find("chunked") != std::string_view::npos;
}

std::optional<size_t> header::content_length() const {
  return field_as<size_t>("Content-Length");
}

} // namespace caf::net::http
