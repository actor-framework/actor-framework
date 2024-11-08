// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_writer.hpp"

#include "caf/detail/append_hex.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/print.hpp"
#include "caf/format_to_error.hpp"

namespace caf {

namespace {

static constexpr const char class_name[] = "caf::json_writer";

constexpr std::string_view json_type_names[] = {"element", "object", "member",
                                                "array",   "string", "number",
                                                "bool",    "null"};

char last_non_ws_char(const std::vector<char>& buf) {
  auto not_ws = [](char c) { return !std::isspace(c); };
  auto last = buf.rend();
  auto i = std::find_if(buf.rbegin(), last, not_ws);
  return (i != last) ? *i : '\0';
}

} // namespace

// -- implementation details ---------------------------------------------------

template <class T>
bool json_writer::number(T x) {
  switch (top()) {
    case type::element:
      detail::print(buf_, x);
      pop();
      return true;
    case type::key:
      add('"');
      detail::print(buf_, x);
      add("\": ");
      return true;
    case type::array:
      sep();
      detail::print(buf_, x);
      return true;
    default:
      fail(type::number);
      return false;
  }
}

// -- constructors, destructors, and assignment operators ----------------------

json_writer::~json_writer() {
  // nop
}

// -- properties ---------------------------------------------------------------

span<const std::byte> json_writer::bytes() const {
  return {reinterpret_cast<const std::byte*>(buf_.data()), buf_.size()};
}

// -- modifiers ----------------------------------------------------------------

void json_writer::reset() {
  buf_.clear();
  stack_.clear();
  push();
}

// -- overrides ----------------------------------------------------------------

bool json_writer::has_human_readable_format() const noexcept {
  return true;
}

bool json_writer::begin_object(type_id_t id, std::string_view name) {
  auto add_type_annotation = [this, id, name] {
    CAF_ASSERT(top() == type::key);
    add(R"_("@type": )_");
    pop();
    CAF_ASSERT(top() == type::element);
    if (auto tname = (*mapper_)(id); !tname.empty()) {
      add('"');
      add(tname);
      add('"');
    } else {
      add('"');
      add(name);
      add('"');
    }
    pop();
    return true;
  };
  if (skip_object_type_annotation_ || inside_object())
    return begin_associative_array(0);
  else
    return begin_associative_array(0) // Put opening paren, ...
           && begin_key_value_pair()  // ... add implicit @type member, ..
           && add_type_annotation()   // ... write content ...
           && end_key_value_pair();   // ... and wait for next field.
}

bool json_writer::end_object() {
  return end_associative_array();
}

bool json_writer::begin_field(std::string_view name) {
  if (begin_key_value_pair()) {
    CAF_ASSERT(top() == type::key);
    add('"');
    add(name);
    add("\": ");
    pop();
    CAF_ASSERT(top() == type::element);
    return true;
  } else {
    return false;
  }
}

bool json_writer::begin_field(std::string_view name, bool is_present) {
  if (skip_empty_fields_ && !is_present) {
    auto t = top();
    switch (t) {
      case type::object:
        push(type::member);
        return true;
      default: {
        std::string str = "expected object, found ";
        str += as_json_type_name(t);
        err_ = format_to_error(sec::runtime_error, "{}::{}: {}", class_name,
                               __func__, str);
        return false;
      }
    }
  } else if (begin_key_value_pair()) {
    CAF_ASSERT(top() == type::key);
    add('"');
    add(name);
    add("\": ");
    pop();
    CAF_ASSERT(top() == type::element);
    if (!is_present) {
      add("null");
      pop();
    }
    return true;
  } else {
    return false;
  }
}

bool json_writer::begin_field(std::string_view name,
                              span<const type_id_t> types, size_t index) {
  if (index >= types.size()) {
    err_ = make_error(sec::runtime_error, "index >= types.size()");
    return false;
  }
  if (begin_key_value_pair()) {
    CAF_ASSERT(top() == type::key);
    add("\"@");
    add(name);
    add(field_type_suffix_);
    add("\": ");
    pop();
    CAF_ASSERT(top() == type::element);
    pop();
    if (auto tname = (*mapper_)(types[index]); !tname.empty()) {
      add('"');
      add(tname);
      add('"');
    } else {
      err_ = make_error(sec::runtime_error, "failed to retrieve type name");
      return false;
    }
    return end_key_value_pair() && begin_field(name);
  } else {
    return false;
  }
}

bool json_writer::begin_field(std::string_view name, bool is_present,
                              span<const type_id_t> types, size_t index) {
  if (is_present)
    return begin_field(name, types, index);
  else
    return begin_field(name, is_present);
}

bool json_writer::end_field() {
  return end_key_value_pair();
}

bool json_writer::begin_tuple(size_t size) {
  return begin_sequence(size);
}

bool json_writer::end_tuple() {
  return end_sequence();
}

bool json_writer::begin_key_value_pair() {
  sep();
  auto t = top();
  switch (t) {
    case type::object:
      push(type::member);
      push(type::element);
      push(type::key);
      return true;
    default: {
      std::string str = "expected object, found ";
      str += as_json_type_name(t);
      err_ = format_to_error(sec::runtime_error, "{}::{}: {}", class_name,
                             __func__, std::move(str));
      return false;
    }
  }
}

bool json_writer::end_key_value_pair() {
  return pop_if(type::member);
}

bool json_writer::begin_sequence(size_t) {
  switch (top()) {
    default:
      err_ = make_error(sec::runtime_error, "unexpected begin_sequence");
      return false;
    case type::element:
      unsafe_morph(type::array);
      break;
    case type::array:
      push(type::array);
      break;
  }
  add('[');
  ++indentation_level_;
  nl();
  return true;
}

bool json_writer::end_sequence() {
  if (pop_if(type::array)) {
    --indentation_level_;
    // Check whether the array was empty and compress the output in that case.
    if (last_non_ws_char(buf_) == '[') {
      while (std::isspace(buf_.back()))
        buf_.pop_back();
    } else {
      nl();
    }
    add(']');
    return true;
  } else {
    return false;
  }
}

bool json_writer::begin_associative_array(size_t) {
  switch (top()) {
    default:
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: unexpected begin_object "
                             "or begin_associative_array",
                             class_name, __func__);
      return false;
    case type::element:
      unsafe_morph(type::object);
      break;
    case type::array:
      sep();
      push(type::object);
      break;
  }
  add('{');
  ++indentation_level_;
  nl();
  return true;
}

bool json_writer::end_associative_array() {
  if (pop_if(type::object)) {
    --indentation_level_;
    // Check whether the array was empty and compress the output in that case.
    if (last_non_ws_char(buf_) == '{') {
      while (std::isspace(buf_.back()))
        buf_.pop_back();
    } else {
      nl();
    }
    add('}');
    if (!stack_.empty())
      stack_.back().filled = true;
    return true;
  } else {
    return false;
  }
}

bool json_writer::value(std::byte x) {
  return number(std::to_integer<uint8_t>(x));
}

bool json_writer::value(bool x) {
  auto add_str = [this, x] {
    if (x)
      add("true");
    else
      add("false");
  };
  switch (top()) {
    case type::element:
      add_str();
      pop();
      return true;
    case type::key:
      add('"');
      add_str();
      add("\": ");
      return true;
    case type::array:
      sep();
      add_str();
      return true;
    default:
      fail(type::boolean);
      return false;
  }
}

bool json_writer::value(int8_t x) {
  return number(x);
}

bool json_writer::value(uint8_t x) {
  return number(x);
}

bool json_writer::value(int16_t x) {
  return number(x);
}

bool json_writer::value(uint16_t x) {
  return number(x);
}

bool json_writer::value(int32_t x) {
  return number(x);
}

bool json_writer::value(uint32_t x) {
  return number(x);
}

bool json_writer::value(int64_t x) {
  return number(x);
}

bool json_writer::value(uint64_t x) {
  return number(x);
}

bool json_writer::value(float x) {
  return number(x);
}

bool json_writer::value(double x) {
  return number(x);
}

bool json_writer::value(long double x) {
  return number(x);
}

bool json_writer::value(std::string_view x) {
  switch (top()) {
    case type::element:
      detail::print_escaped(buf_, x);
      pop();
      return true;
    case type::key:
      detail::print_escaped(buf_, x);
      add(": ");
      pop();
      return true;
    case type::array:
      sep();
      detail::print_escaped(buf_, x);
      return true;
    default:
      fail(type::string);
      return false;
  }
}

bool json_writer::value(const std::u16string&) {
  err_ = make_error(sec::unsupported_operation,
                    "u16string not supported yet by caf::json_writer");
  return false;
}

bool json_writer::value(const std::u32string&) {
  err_ = make_error(sec::unsupported_operation,
                    "u32string not supported yet by caf::json_writer");
  return false;
}

bool json_writer::value(span<const std::byte> x) {
  switch (top()) {
    case type::element:
      add('"');
      detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                         x.size());
      add('"');
      pop();
      return true;
    case type::key:
      unsafe_morph(type::element);
      add('"');
      detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                         x.size());
      add("\": ");
      pop();
      return true;
    case type::array:
      sep();
      add('"');
      detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                         x.size());
      add('"');
      return true;
    default:
      fail(type::string);
      return false;
  }
}

// -- state management ---------------------------------------------------------

void json_writer::init() {
  // Reserve some reasonable storage for the character buffer. JSON grows
  // quickly, so we can start at 1kb to avoid a couple of small allocations in
  // the beginning.
  buf_.reserve(1024);
  // Even heavily nested objects should fit into 32 levels of nesting.
  stack_.reserve(32);
  // Placeholder for what is to come.
  push();
}

json_writer::type json_writer::top() {
  if (!stack_.empty())
    return stack_.back().t;
  else
    return type::null;
}

// Enters a new level of nesting.
void json_writer::push(type t) {
  auto tmp = entry{t, false};
  stack_.push_back(tmp);
}

// Backs up one level of nesting.
bool json_writer::pop() {
  if (!stack_.empty()) {
    stack_.pop_back();
    return true;
  }
  err_ = make_error(sec::runtime_error,
                    "pop() called with an empty stack: begin/end mismatch");
  return false;
}

bool json_writer::pop_if(type t) {
  if (!stack_.empty() && stack_.back() == t) {
    stack_.pop_back();
    return true;
  }
  if (stack_.empty()) {
    err_ = format_to_error(sec::runtime_error,
                           "pop_if failed: expected {} "
                           "but found an empty stack",
                           as_json_type_name(t));
  } else {
    err_ = format_to_error(sec::runtime_error,
                           "pop_if failed: expected {} but found {}",
                           as_json_type_name(t),
                           as_json_type_name(stack_.back().t));
  }
  return false;
}

bool json_writer::pop_if_next(type t) {
  if (stack_.size() > 1
      && (stack_[stack_.size() - 2] == t
          || can_morph(stack_[stack_.size() - 2].t, t))) {
    stack_.pop_back();
    return true;
  }
  if (stack_.size() < 2) {
    err_ = format_to_error(sec::runtime_error,
                           "pop_if_next failed: expected {} "
                           "but found a stack of size",
                           as_json_type_name(t), stack_.size());
  } else {
    err_ = format_to_error(sec::runtime_error,
                           "pop_if_next failed: expected {} but found {}",
                           as_json_type_name(t),
                           as_json_type_name(stack_[stack_.size() - 2].t));
  }
  return false;
}

// Tries to morph the current top of the stack to t.
bool json_writer::morph(type t) {
  type unused;
  return morph(t, unused);
}

bool json_writer::morph(type t, type& prev) {
  if (!stack_.empty()) {
    if (can_morph(stack_.back().t, t)) {
      prev = stack_.back().t;
      stack_.back().t = t;
      return true;
    }
    std::string str = "cannot convert ";
    str += as_json_type_name(stack_.back().t);
    str += " to ";
    str += as_json_type_name(t);
    err_ = make_error(sec::runtime_error, std::move(str));
    return false;
  }
  std::string str = "mismatched begin/end calls on the JSON inspector";
  err_ = make_error(sec::runtime_error, std::move(str));
  return false;
}

void json_writer::unsafe_morph(type t) {
  stack_.back().t = t;
}

void json_writer::fail(type t) {
  err_ = format_to_error(sec::runtime_error,
                         "failed to write a {}: "
                         "invalid position (begin/end mismatch?)",
                         as_json_type_name(t));
}

bool json_writer::inside_object() const noexcept {
  auto is_object = [](const entry& x) { return x.t == type::object; };
  return std::any_of(stack_.begin(), stack_.end(), is_object);
}

// -- printing ---------------------------------------------------------------

void json_writer::nl() {
  if (indentation_factor_ > 0) {
    buf_.push_back('\n');
    buf_.insert(buf_.end(), indentation_factor_ * indentation_level_, ' ');
  }
}

void json_writer::sep() {
  CAF_ASSERT(top() == type::element || top() == type::object
             || top() == type::array);
  if (stack_.back().filled) {
    if (indentation_factor_ > 0) {
      add(",\n");
      buf_.insert(buf_.end(), indentation_factor_ * indentation_level_, ' ');
    } else {
      add(", ");
    }
  } else {
    stack_.back().filled = true;
  }
}

// -- free functions -----------------------------------------------------------

std::string_view as_json_type_name(json_writer::type t) {
  return json_type_names[static_cast<uint8_t>(t)];
}

} // namespace caf
