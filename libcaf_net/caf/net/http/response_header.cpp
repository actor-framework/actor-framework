#include "caf/net/http/response_header.hpp"

#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::net::http {

namespace {

constexpr std::string_view eol = "\r\n";

// Applies f to all lines separated by eol until an empty line.
template <class F>
expected<std::string_view> for_each_line(std::string_view input, F&& f) {
  for (auto pos = input.begin();;) {
    auto line_end = std::search(pos, input.end(), eol.begin(), eol.end());
    if (line_end == input.end()) {
      // No delimiter found
      return make_error(sec::logic_error, "Malformed header section");
    }
    auto to_line_end = std::distance(pos, line_end);
    CAF_ASSERT(to_line_end >= 0);
    auto line = std::string_view{pos, static_cast<size_t>(to_line_end)};
    pos = line_end + eol.size();
    if (line.empty()) {
      // mepty line headers and body
      auto to_line_end = static_cast<size_t>(std::distance(pos, input.end()));
      return std::string_view{pos, to_line_end};
    }
    if (!f(line))
      return make_error(sec::logic_error);
  }
}

// Remove once we get c++20
bool starts_with(std::string_view str, std::string_view seek) {
  return str.find(seek) == 0;
}

// Validate the version string.
bool validate_http_version(std::string_view str) {
  using namespace std::literals;
  auto begin = "HTTP/"sv;
  if (starts_with(str, begin))
    str.remove_prefix(begin.size());
  if (str == "1.0")
    return true;
  if (str == "1.1")
    return true;
  // We dont support HTTP 2 and 3, and we pretend like HTTP/0.9 doesn't exist.
  return false;
}

} // namespace

response_header::response_header(const response_header& other) {
  assign(other);
}

response_header& response_header::operator=(const response_header& other) {
  assign(other);
  return *this;
}

void response_header::assign(const response_header& other) {
  auto remap = [](const char* base, std::string_view src,
                  const char* new_base) {
    auto offset = std::distance(base, src.data());
    return std::string_view{new_base + offset, src.size()};
  };
  if (other.valid()) {
    raw_.assign(other.raw_.begin(), other.raw_.end());
    auto base = other.raw_.data();
    auto new_base = raw_.data();
    version_ = remap(base, other.version_, new_base);
    status_ = other.status_;
    status_text_ = remap(base, other.status_text_, new_base);
    fields_.resize(other.fields_.size());
    for (size_t index = 0; index < fields_.size(); ++index) {
      fields_[index].first = remap(base, other.fields_[index].first, new_base);
      fields_[index].second = remap(base, other.fields_[index].second,
                                    new_base);
    }
    body_ = remap(base, other.body_, new_base);
  } else {
    raw_.clear();
    version_ = std::string_view{};
    fields_.clear();
  }
}

bool response_header::chunked_transfer_encoding() const {
  return field("Transfer-Encoding").find("chunked") != std::string_view::npos;
}

std::optional<size_t> response_header::content_length() const {
  return field_as<size_t>("Content-Length");
}

std::pair<status, std::string_view>
response_header::parse(std::string_view raw) {
  using namespace literals;
  CAF_LOG_TRACE(CAF_ARG(raw));
  // Sanity checking and copying of the raw input.
  if (raw.empty()) {
    raw_.clear();
    return {status::bad_request, "Empty header."};
  };
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
  // TODO: consider allowing statuses by range instead by enum values
  // Parse the status from the string
  if (auto res = get_as<uint16_t>(config_value{status_str});
      !res.has_value() || !from_integer(*res, status_)) {
    CAF_LOG_DEBUG("Invalid status");
    raw_.clear();
    return {status::bad_request, "Invalid HTTP status."};
  }
  if (status_text.empty()) {
    CAF_LOG_DEBUG("Empty status text.");
    raw_.clear();
    return {status::bad_request, "Invalid HTTP status text."};
  }
  status_text_ = status_text;
  fields_.clear();
  auto ok = for_each_line(remainder, [this](std::string_view line) {
    if (auto sep = std::find(line.begin(), line.end(), ':');
        sep != line.end()) {
      auto n = static_cast<size_t>(std::distance(line.begin(), sep));
      auto key = trim(std::string_view{line.data(), n});
      auto m = static_cast<size_t>(std::distance(sep + 1, line.end()));
      auto val = trim(std::string_view{std::addressof(*(sep + 1)), m});
      if (!key.empty()) {
        fields_.emplace_back(key, val);
        return true;
      }
    }
    return false;
  });
  // If set, ok holds the remaining input from the raw.
  if (ok) {
    body_ = *ok;
    return {status::ok, "OK"};
  } else {
    raw_.clear();
    version_ = std::string_view{};
    status_text_ = std::string_view{};
    fields_.clear();
    return {status::bad_request, "Malformed header fields."};
  }
}

} // namespace caf::net::http
