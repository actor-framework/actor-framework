#include "caf/net/http/header.hpp"

#include "caf/expected.hpp"
#include "caf/logger.hpp"

namespace caf::net::http {

namespace {

constexpr string_view eol = "\r\n";

template <class F>
bool for_each_line(string_view input, F&& f) {
  for (auto pos = input.begin();;) {
    auto line_end = std::search(pos, input.end(), eol.begin(), eol.end());
    if (line_end == input.end() || std::distance(pos, input.end()) == 2) {
      // Reaching the end or hitting the last empty line tells us we're done.
      return true;
    }
    if (!f(string_view{pos, line_end}))
      return false;
    pos = line_end + eol.size();
  }
  return true;
}

string_view trim(string_view str) {
  str.remove_prefix(std::min(str.find_first_not_of(' '), str.size()));
  auto trim_pos = str.find_last_not_of(' ');
  if (trim_pos != str.npos)
    str.remove_suffix(str.size() - (trim_pos + 1));
  return str;
}

/// Splits `str` at the first occurrence of `sep` into the head and the
/// remainder (excluding the separator).
static std::pair<string_view, string_view> split(string_view str,
                                                 string_view sep) {
  auto i = std::search(str.begin(), str.end(), sep.begin(), sep.end());
  if (i != str.end())
    return {{str.begin(), i}, {i + sep.size(), str.end()}};
  return {{str}, {}};
}

/// Convenience function for splitting twice.
static std::tuple<string_view, string_view, string_view>
split2(string_view str, string_view sep) {
  auto [first, r1] = split(str, sep);
  auto [second, third] = split(r1, sep);
  return {first, second, third};
}

/// @pre `y` is all lowercase
bool case_insensitive_eq(string_view x, string_view y) {
  return std::equal(x.begin(), x.end(), y.begin(), y.end(),
                    [](char a, char b) { return tolower(a) == b; });
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
  auto remap = [](const char* base, string_view src, const char* new_base) {
    auto offset = std::distance(base, src.data());
    return string_view{new_base + offset, src.size()};
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
    version_ = string_view{};
    fields_.clear();
  }
}

std::pair<status, string_view> header::parse(string_view raw) {
  CAF_LOG_TRACE(CAF_ARG(raw));
  // Sanity checking and copying of the raw input.
  using namespace literals;
  if (raw.empty()) {
    raw_.clear();
    return {status::bad_request, "Empty header."};
  };
  raw_.assign(raw.begin(), raw.end());
  // Parse the first line, i.e., "METHOD REQUEST-URI VERSION".
  auto [first_line, remainder] = split(string_view{raw_.data(), raw_.size()},
                                       eol);
  auto [method_str, request_uri_str, version] = split2(first_line, " ");
  // The path must be absolute.
  if (request_uri_str.empty() || request_uri_str.front() != '/') {
    CAF_LOG_DEBUG("Malformed Request-URI: expected an absolute path.");
    raw_.clear();
    return {status::bad_request,
            "Malformed Request-URI: expected an absolute path."};
  }
  // The path must form a valid URI when prefixing a scheme. We don't actually
  // care about the scheme for the validation step.
  if (auto res = make_uri("nil:" + to_string(request_uri_str))) {
    uri_ = std::move(*res);
  } else {
    CAF_LOG_DEBUG("Failed to parse URI" << request_uri_str << "->"
                                        << res.error());
    raw_.clear();
    return {status::bad_request, "Malformed Request-URI."};
  }
  // Verify and store the method.
  if (case_insensitive_eq(method_str, "get")) {
    method_ = method::get;
  } else if (case_insensitive_eq(method_str, "head")) {
    method_ = method::head;
  } else if (case_insensitive_eq(method_str, "post")) {
    method_ = method::post;
  } else if (case_insensitive_eq(method_str, "put")) {
    method_ = method::put;
  } else if (case_insensitive_eq(method_str, "delete")) {
    method_ = method::del;
  } else if (case_insensitive_eq(method_str, "connect")) {
    method_ = method::connect;
  } else if (case_insensitive_eq(method_str, "options")) {
    method_ = method::options;
  } else if (case_insensitive_eq(method_str, "trace")) {
    method_ = method::trace;
  } else {
    CAF_LOG_DEBUG("Invalid HTTP method.");
    raw_.clear();
    return {status::bad_request, "Invalid HTTP method."};
  }
  // Store the remaining header fields.
  version_ = version;
  fields_.clear();
  bool ok = for_each_line(remainder, [this](string_view line) {
    if (auto sep = std::find(line.begin(), line.end(), ':');
        sep != line.end()) {
      auto key = trim({line.begin(), sep});
      auto val = trim({sep + 1, line.end()});
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
    version_ = string_view{};
    fields_.clear();
    return {status::bad_request, "Malformed header fields."};
  }
}

bool header::chunked_transfer_encoding() const {
  return field("Transfer-Encoding").find("chunked") != string_view::npos;
}

optional<size_t> header::content_length() const {
  return field_as<size_t>("Content-Length");
}

} // namespace caf::net::http
