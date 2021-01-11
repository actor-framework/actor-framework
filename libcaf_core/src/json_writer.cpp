// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_writer.hpp"

#include "caf/detail/append_hex.hpp"
#include "caf/detail/print.hpp"

namespace caf {

namespace {

constexpr bool can_morph(json_writer::type from, json_writer::type to) {
  return from == json_writer::type::element && to != json_writer::type::member;
}

constexpr const char* json_type_names[] = {"element", "object", "member",
                                           "array",   "string", "number",
                                           "bool",    "null"};

constexpr const char* json_type_name(json_writer::type t) {
  return json_type_names[static_cast<uint8_t>(t)];
}

} // namespace

// -- implementation details ---------------------------------------------------

template <class T>
bool json_writer::number(T x) {
  switch (top()) {
    case type::element:
      unsafe_morph(type::number);
      detail::print(buf_, x);
      return true;
    case type::member:
      unsafe_morph(type::element);
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

json_writer::json_writer() : json_writer(nullptr) {
  init();
}

json_writer::json_writer(actor_system& sys) : super(sys) {
  init();
}

json_writer::json_writer(execution_unit* ctx) : super(ctx) {
  init();
}

json_writer::~json_writer() {
  // nop
}

// -- modifiers ----------------------------------------------------------------

void json_writer::reset() {
  buf_.clear();
  stack_.clear();
  push();
}

// -- overrides ----------------------------------------------------------------

bool json_writer::begin_object(type_id_t, string_view name) {
  auto add_type_annotation = [this, name] {
    add(R"_("@type": )_");
    detail::print_escaped(buf_, name);
    return true;
  };
  return begin_associative_array(0) // Put opening paren, ...
         && begin_key_value_pair()  // ... add implicit @type member, ..
         && add_type_annotation()   // ... write content ...
         && end_key_value_pair();   // ... and wait for next field.
}

bool json_writer::end_object() {
  return end_associative_array();
}

bool json_writer::begin_field(string_view name) {
  if (begin_key_value_pair()) {
    detail::print_escaped(buf_, name);
    add(": ");
    unsafe_morph(type::element);
    return true;
  } else {
    return false;
  }
}

bool json_writer::begin_field(string_view name, bool is_present) {
  if (begin_key_value_pair()) {
    detail::print_escaped(buf_, name);
    add(": ");
    if (is_present)
      unsafe_morph(type::element);
    else
      add("null");
    return true;
  } else {
    return false;
  }
}

bool json_writer::begin_field(string_view name, span<const type_id_t>, size_t) {
  return begin_field(name);
}

bool json_writer::begin_field(string_view name, bool is_present,
                              span<const type_id_t>, size_t) {
  return begin_field(name, is_present);
}

bool json_writer::end_field() {
  return pop_if_next(type::object);
}

bool json_writer::begin_tuple(size_t size) {
  return begin_sequence(size);
}

bool json_writer::end_tuple() {
  return end_sequence();
}

bool json_writer::begin_key_value_pair() {
  sep();
  return push_if(type::object, type::member);
}

bool json_writer::end_key_value_pair() {
  // Note: can't check for `type::member` here, because it has been morphed to
  // whatever the value type has been. But after popping the current type, there
  // still has to be the `object` entry below.
  return pop_if_next(type::object);
}

bool json_writer::begin_sequence(size_t) {
  switch (top()) {
    default:
      emplace_error(sec::runtime_error, "unexpected begin_sequence");
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
    nl();
    add(']');
    return true;
  } else {
    return false;
  }
}

bool json_writer::begin_associative_array(size_t) {
  switch (top()) {
    default:
      emplace_error(sec::runtime_error,
                    "unexpected begin_object or begin_associative_array");
      return false;
    case type::member:
      emplace_error(sec::runtime_error,
                    "unimplemented: json_writer currently does not support "
                    "complex types as dictionary keys");
      return false;
    case type::element:
      unsafe_morph(type::object);
      break;
    case type::array:
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
    nl();
    add('}');
    if (top() == type::array)
      sep();
    return true;
  } else {
    return false;
  }
}

bool json_writer::value(byte x) {
  return number(to_integer<uint8_t>(x));
}

bool json_writer::value(bool x) {
  return number(x);
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

bool json_writer::value(string_view x) {
  switch (top()) {
    case type::element:
      unsafe_morph(type::string);
      detail::print_escaped(buf_, x);
      return true;
    case type::member:
      unsafe_morph(type::element);
      detail::print_escaped(buf_, x);
      add(": ");
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
  emplace_error(sec::unsupported_operation,
                "u16string not supported yet by caf::json_writer");
  return false;
}

bool json_writer::value(const std::u32string&) {
  emplace_error(sec::unsupported_operation,
                "u32string not supported yet by caf::json_writer");
  return false;
}

bool json_writer::value(span<const byte> x) {
  switch (top()) {
    case type::element:
      unsafe_morph(type::string);
      add('"');
      detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                         x.size());
      add('"');
      return true;
    case type::member:
      unsafe_morph(type::element);
      add('"');
      detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                         x.size());
      add("\": ");
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
  has_human_readable_format_ = true;
  // Reserve some reasonable storage for the character buffer. JSON grows
  // quickly, so we can start at 1kb to avoid a couple of small allocations in
  // the beginning.
  buf_.reserve(1024);
  // Even heavily nested objects should fit into 32 levels of nesting.
  stack_.reserve(32);
  // The concrete type of the output is yet unknown.
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
  stack_.push_back({t, false});
}

bool json_writer::push_if(type expected, type t) {
  if (!stack_.empty() && stack_.back() == expected) {
    stack_.push_back({t, false});
    return true;
  } else {
    std::string str = "push_if failed: expected = ";
    str += json_type_name(expected);
    str += ", found = ";
    str += json_type_name(t);
    emplace_error(sec::runtime_error, std::move(str));
    return false;
  }
}

// Backs up one level of nesting.
bool json_writer::pop() {
  if (!stack_.empty()) {
    stack_.pop_back();
    return true;
  } else {
    std::string str = "pop() called with an empty stack: begin/end mismatch";
    emplace_error(sec::runtime_error, std::move(str));
    return false;
  }
}

bool json_writer::pop_if(type t) {
  if (!stack_.empty() && stack_.back() == t) {
    stack_.pop_back();
    return true;
  } else {
    std::string str = "pop_if failed: expected = ";
    str += json_type_name(t);
    if (stack_.empty()) {
      str += ", found an empty stack";
    } else {
      str += ", found = ";
      str += json_type_name(stack_.back().t);
    }
    emplace_error(sec::runtime_error, std::move(str));
    return false;
  }
}

bool json_writer::pop_if_next(type t) {
  if (stack_.size() > 1 && stack_[stack_.size() - 2] == t) {
    stack_.pop_back();
    return true;
  } else {
    std::string str = "pop_if_next failed: expected = ";
    str += json_type_name(t);
    if (stack_.size() < 2) {
      str += ", found a stack of size ";
      detail::print(str, stack_.size());
    } else {
      str += ", found = ";
      str += json_type_name(stack_[stack_.size() - 2].t);
    }
    emplace_error(sec::runtime_error, std::move(str));
    return false;
  }
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
    } else {
      std::string str = "cannot convert ";
      str += json_type_name(stack_.back().t);
      str += " to ";
      str += json_type_name(t);
      emplace_error(sec::runtime_error, std::move(str));
      return false;
    }
  } else {
    std::string str = "mismatched begin/end calls on the JSON inspector";
    emplace_error(sec::runtime_error, std::move(str));
    return false;
  }
}

void json_writer::unsafe_morph(type t) {
  stack_.back().t = t;
}

void json_writer::fail(type t) {
  std::string str = "failed to write a ";
  str += json_type_name(t);
  str += ": invalid position (begin/end mismatch?)";
  emplace_error(sec::runtime_error, std::move(str));
}

// -- printing ---------------------------------------------------------------

void json_writer::nl() {
  if (indentation_factor_ > 0) {
    buf_.push_back('\n');
    buf_.insert(buf_.end(), indentation_factor_ * indentation_level_, ' ');
  }
}

void json_writer::sep() {
  CAF_ASSERT(top() == type::object || top() == type::array);
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

} // namespace caf
