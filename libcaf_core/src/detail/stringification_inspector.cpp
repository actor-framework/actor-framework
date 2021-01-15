// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/stringification_inspector.hpp"

#include "caf/detail/print.hpp"

#include <algorithm>
#include <ctime>

namespace caf::detail {

bool stringification_inspector::begin_object(type_id_t, string_view name) {
  sep();
  if (name != "std::string") {
    result_.insert(result_.end(), name.begin(), name.end());
    result_ += '(';
  } else {
    in_string_object_ = true;
  }
  return true;
}

bool stringification_inspector::end_object() {
  if (!in_string_object_)
    result_ += ')';
  else
    in_string_object_ = false;
  return true;
}

bool stringification_inspector::begin_field(string_view) {
  return true;
}

bool stringification_inspector::begin_field(string_view, bool is_present) {
  sep();
  if (!is_present)
    result_ += "null";
  else
    result_ += '*';
  return true;
}

bool stringification_inspector::begin_field(string_view, span<const type_id_t>,
                                            size_t) {
  return true;
}

bool stringification_inspector::begin_field(string_view, bool is_present,
                                            span<const type_id_t>, size_t) {
  sep();
  if (!is_present)
    result_ += "null";
  else
    result_ += '*';
  return true;
}

bool stringification_inspector::end_field() {
  return true;
}

bool stringification_inspector::begin_sequence(size_t) {
  sep();
  result_ += '[';
  return true;
}

bool stringification_inspector::end_sequence() {
  result_ += ']';
  return true;
}

bool stringification_inspector::value(byte x) {
  return value(span<const byte>(&x, 1));
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
  detail::print(result_, x);
  return true;
}

bool stringification_inspector::value(long double x) {
  sep();
  detail::print(result_, x);
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
    detail::print_escaped(result_, str);
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

bool stringification_inspector::value(span<const byte> x) {
  sep();
  detail::append_hex(result_, x.data(), x.size());
  return true;
}

bool stringification_inspector::list(const std::vector<bool>& xs) {
  begin_sequence(xs.size());
  for (bool x : xs)
    value(x);
  return end_sequence();
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
