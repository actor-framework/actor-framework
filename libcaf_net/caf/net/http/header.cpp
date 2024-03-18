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
// if no eol is found or `f` returns false.
template <class F>
expected<std::string_view> process_lines(std::string_view input, F&& f) {
  for (;;) {
    size_t pos = input.find(eol);
    // Stop when not finding the delimiter.
    if (pos == std::string_view::npos)
      return error{sec::logic_error, "EOL delimiter not found"};
    // Stop at the first empty line and return the remainder.
    if (pos == 0) {
      input.remove_prefix(eol.size());
      return {input};
    }
    // Stop if the predicate returns false.
    if (!f(input.substr(0, pos)))
      return error{sec::logic_error, "Predicate function failed"};
    // Drop the consumed line from the input for the next iteration.
    input.remove_prefix(pos + eol.size());
  }
}

} // namespace

header::~header() {
  // nop
}

header::header(const header& other) {
  if (!other.raw_.empty()) {
    raw_.assign(other.raw_.begin(), other.raw_.end());
    reassign_fields(other);
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

void header::reassign_fields(const header& other) {
  auto base = other.raw_.data();
  auto new_base = raw_.data();
  fields_.resize(other.fields_.size());
  for (size_t index = 0; index < fields_.size(); ++index) {
    fields_[index].first = remap(base, other.fields_[index].first, new_base);
    fields_[index].second = remap(base, other.fields_[index].second, new_base);
  }
}

void header::clear() noexcept {
  fields_.clear();
  raw_.clear();
}

// Note: does not take ownership of the data.
expected<std::string_view> header::parse_fields(std::string_view data) {
  auto remainder = process_lines(data, [this](std::string_view line) {
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
  if (!remainder)
    clear();
  return remainder;
}

bool header::chunked_transfer_encoding() const noexcept {
  return field("Transfer-Encoding").find("chunked") != std::string_view::npos;
}

std::optional<size_t> header::content_length() const noexcept {
  return field_as<size_t>("Content-Length");
}

} // namespace caf::net::http
