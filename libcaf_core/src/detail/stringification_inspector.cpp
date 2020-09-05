/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/stringification_inspector.hpp"

#include "caf/detail/print.hpp"

#include <algorithm>
#include <ctime>

namespace {

void escape(std::string& result, char c) {
  switch (c) {
    default:
      result += c;
      break;
    case '\n':
      result += R"(\n)";
      break;
    case '\t':
      result += R"(\t)";
      break;
    case '\\':
      result += R"(\\)";
      break;
    case '"':
      result += R"(\")";
      break;
  }
}

} // namespace

namespace caf::detail {

bool stringification_inspector::begin_object(string_view name) {
  sep();
  result_.insert(result_.end(), name.begin(), name.end());
  result_ += '(';
  return ok;
}

bool stringification_inspector::end_object() {
  result_ += ')';
  return ok;
}

bool stringification_inspector::begin_field(string_view) {
  return ok;
}

bool stringification_inspector::begin_field(string_view, bool is_present) {
  sep();
  if (!is_present)
    result_ += "null";
  else
    result_ += '*';
  return ok;
}

bool stringification_inspector::begin_field(string_view, span<const type_id_t>,
                                            size_t) {
  return ok;
}

bool stringification_inspector::begin_field(string_view, bool is_present,
                                            span<const type_id_t>, size_t) {
  sep();
  if (!is_present)
    result_ += "null";
  else
    result_ += '*';
  return ok;
}

bool stringification_inspector::end_field() {
  return ok;
}

bool stringification_inspector::begin_sequence(size_t) {
  sep();
  result_ += '[';
  return ok;
}

bool stringification_inspector::end_sequence() {
  result_ += ']';
  return ok;
}

bool stringification_inspector::value(bool x) {
  sep();
  result_ += x ? "true" : "false";
  return true;
}

bool stringification_inspector::value(float x) {
  sep();
  auto str = std::to_string(x);
  result_ += str;
  return true;
}

bool stringification_inspector::value(double x) {
  sep();
  auto str = std::to_string(x);
  result_ += str;
  return true;
}

bool stringification_inspector::value(long double x) {
  sep();
  auto str = std::to_string(x);
  result_ += str;
  return true;
}

namespace {

template <class Duration>
auto dcast(timespan x) {
  return std::chrono::duration_cast<Duration>(x);
}

} // namespace

bool stringification_inspector::value(timespan x) {
  namespace sc = std::chrono;
  sep();
  auto try_print = [this](auto converted, const char* suffix) {
    if (converted.count() < 1)
      return false;
    value(converted.count());
    result_ += suffix;
    return true;
  };
  if (try_print(dcast<sc::hours>(x), "h")
      || try_print(dcast<sc::minutes>(x), "min")
      || try_print(dcast<sc::seconds>(x), "s")
      || try_print(dcast<sc::milliseconds>(x), "ms")
      || try_print(dcast<sc::microseconds>(x), "us"))
    return true;
  value(x.count());
  result_ += "ns";
  return true;
}

bool stringification_inspector::value(timestamp x) {
  sep();
  append_timestamp_to_string(result_, x);
  return true;
}

bool stringification_inspector::value(string_view str) {
  sep();
  if (str.empty()) {
    result_ += R"("")";
    return true;
  }
  if (str[0] == '"') {
    // Assume an already escaped string.
    result_.insert(result_.end(), str.begin(), str.end());
    return true;
  }
  // Escape the string if it contains whitespaces or characters that need
  // escaping.
  auto needs_escaping = [](char c) {
    return isspace(c) || c == '\\' || c == '"';
  };
  if (always_quote_strings
      || std::any_of(str.begin(), str.end(), needs_escaping)) {
    result_ += '"';
    for (char c : str)
      escape(result_, c);
    result_ += '"';
  } else {
    result_.insert(result_.end(), str.begin(), str.end());
  }
  return true;
}

bool stringification_inspector::value(const std::u16string&) {
  sep();
  // Convert to UTF-8 and print?
  result_ += "<unprintable>";
  return true;
}

bool stringification_inspector::value(const std::u32string&) {
  sep();
  // Convert to UTF-8 and print?
  result_ += "<unprintable>";
  return true;
}

bool stringification_inspector::int_value(int64_t x) {
  sep();
  detail::print(result_, x);
  return true;
}

bool stringification_inspector::int_value(uint64_t x) {
  sep();
  detail::print(result_, x);
  return true;
}

bool stringification_inspector::value(const std::vector<bool>& xs) {
  begin_sequence(xs.size());
  for (bool x : xs)
    value(x);
  return end_sequence();
}

bool stringification_inspector::value(const char* x) {
  return value(string_view{x, strlen(x)});
}

void stringification_inspector::sep() {
  if (!result_.empty())
    switch (result_.back()) {
      case '(':
      case '[':
      case '{':
      case '*':
      case ' ': // only at back if we've printed ", " before
        break;
      default:
        result_ += ", ";
    }
}

} // namespace caf::detail
