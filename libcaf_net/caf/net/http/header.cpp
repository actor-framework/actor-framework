// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/http/header.hpp"

#include "caf/logger.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::net::http {

namespace {

constexpr std::string_view eol = "\r\n";

// Iterates through lines separated by eol and applies f to them, until an empty
// line is found (containing only eol). Returns the unprocessed input or error
// if no eol is found or f fails.
template <class F>
expected<std::string_view> for_each_line(std::string_view input, F&& f) {
  std::cout << "FOR EACH LINE ARG " << input << std::endl;
  for (auto pos = input.begin();;) {
    std::cout << "FOR LOOP" << std::endl;
    if (pos == input.begin()) {
      std::cout << "pos is beginning" << std::endl;
    } else if (pos > input.begin() && pos < input.end()) {
      std::cout << "pos beginning end" << std::endl;
    } else if (pos >= input.end()) {
      std::cout << "pos after end" << std::endl;
    }
    auto line_end = std::search(pos, input.end(), eol.begin(), eol.end());
    if (line_end == input.end()) {
      std::cout << "EOL not found" << std::endl;
      return make_error(sec::logic_error, "EOL delimiter not found");
    }
    auto to_line_end = std::distance(pos, line_end);
    std::cout << "Distance is" << to_line_end << std::endl;
    CAF_ASSERT(to_line_end >= 0);
    auto line = std::string_view{std::addressof(*pos),
                                 static_cast<size_t>(to_line_end)};
    std::cout << "Line is" << line << std::endl;
    pos = line_end + eol.size();
    if (line.empty()) {
      std::cout << "Empty line - returning" << std::endl;
      return std::string_view{std::addressof(*pos),
                              static_cast<size_t>(
                                std::distance(pos, input.end()))};
    }
    if (!f(line)) {
      std::cout << "Predicate failed - returning error " << std::endl;
      return make_error(sec::logic_error, "Predicate function failed");
    }
    std::cout << "Predicate success - loop" << std::endl;
  }
}

} // namespace

header::header(const header& other) {
  if (!other.raw_.empty()) {
    raw_.assign(other.raw_.begin(), other.raw_.end());
    reassign_fields(other);
  } else {
    raw_.clear();
    fields_.clear();
  }
}

header& header::operator=(const header& other) {
  if (!other.raw_.empty()) {
    raw_.assign(other.raw_.begin(), other.raw_.end());
    reassign_fields(other);
  } else {
    raw_.clear();
    fields_.clear();
  }
  return *this;
}

void header::reassign_fields(const header& other) noexcept {
  auto base = other.raw_.data();
  auto new_base = raw_.data();
  fields_.resize(other.fields_.size());
  for (size_t index = 0; index < fields_.size(); ++index) {
    fields_[index].first = remap(base, other.fields_[index].first, new_base);
    fields_[index].second = remap(base, other.fields_[index].second, new_base);
  }
}

// Note: does not take ownership of the data.
expected<std::string_view> header::parse_fields(std::string_view data) {
  std::cout << "PARSE PROCESSING OF" << data << std::endl;
  auto remainder = for_each_line(data, [this](std::string_view line) {
    std::cout << "  lambda: arg <" << line << ">" << std::endl;
    if (auto sep = std::find(line.begin(), line.end(), ':');
        sep != line.end()) {
      std::cout << "  lambda if: found" << std::endl;
      auto n = static_cast<size_t>(std::distance(line.begin(), sep));
      auto key = trim(std::string_view{line.data(), n});
      auto m = static_cast<size_t>(std::distance(sep + 1, line.end()));
      auto val = trim(std::string_view{std::addressof(*(sep + 1)), m});
      std::cout << "  lambda if key: " << key << " val: " << val << std::endl;
      if (!key.empty()) {
        fields_.emplace_back(key, val);
        std::cout << "  lambda if returning true" << std::endl;
        return true;
      }
    }
    std::cout << "  lambda if not found returning false" << std::endl;
    return false;
  });
  if (!remainder) {
    std::cout << "PARSE remainder is an error" << std::endl;
    raw_.clear();
    fields_.clear();
  }
  std::cout << "PARSE remainder is " << *remainder << std::endl;
  return remainder;
}

bool header::chunked_transfer_encoding() const {
  return field("Transfer-Encoding").find("chunked") != std::string_view::npos;
}

std::optional<size_t> header::content_length() const {
  return field_as<size_t>("Content-Length");
}

} // namespace caf::net::http
