// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/stringification_inspector.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/detail/print.hpp"

#include <algorithm>
#include <ctime>

namespace caf::detail {

bool stringification_inspector::begin_object(type_id_t, std::string_view name) {
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

bool stringification_inspector::begin_field(std::string_view) {
  return true;
}

bool stringification_inspector::begin_field(std::string_view, bool is_present) {
  sep();
  if (!is_present)
    result_ += "null";
  else
    result_ += '*';
  return true;
}

bool stringification_inspector::begin_field(std::string_view,
                                            span<const type_id_t>, size_t) {
  return true;
}

bool stringification_inspector::begin_field(std::string_view, bool is_present,
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

bool stringification_inspector::value(std::byte x) {
  return value(span<const std::byte>(&x, 1));
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

bool stringification_inspector::value(timespan x) {
  sep();
  detail::print(result_, x);
  return true;
}

bool stringification_inspector::value(timestamp x) {
  sep();
  append_timestamp_to_string(result_, x);
  return true;
}

bool stringification_inspector::value(std::string_view str) {
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
    detail::print_escaped(result_, str);
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

bool stringification_inspector::value(span<const std::byte> x) {
  sep();
  detail::append_hex(result_, x.data(), x.size());
  return true;
}

bool stringification_inspector::value(const strong_actor_ptr& ptr) {
  if (!ptr) {
    sep();
    result_ += "null";
  } else {
    sep();
    detail::print(result_, ptr->id());
    result_ += '@';
    result_ += to_string(ptr->node());
  }
  return true;
}

bool stringification_inspector::value(const weak_actor_ptr& ptr) {
  return value(ptr.lock());
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
