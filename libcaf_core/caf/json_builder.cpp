// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_builder.hpp"

#include "caf/detail/append_hex.hpp"
#include "caf/detail/print.hpp"
#include "caf/json_value.hpp"

#include <type_traits>

using namespace std::literals;

namespace caf {

namespace {

static constexpr const char class_name[] = "caf::json_builder";

} // namespace

// -- implementation details ---------------------------------------------------

template <class T>
bool json_builder::number(T x) {
  switch (top()) {
    case type::element: {
      if constexpr (std::is_floating_point_v<T>) {
        top_ptr()->data = static_cast<double>(x);
      } else {
        top_ptr()->data = static_cast<int64_t>(x);
      }
      pop();
      return true;
    }
    case type::key: {
      std::string str;
      detail::print(str, x);
      *top_ptr<key_type>() = detail::json::realloc(str, &storage_->buf);
      return true;
    }
    case type::array: {
      auto& new_entry = top_ptr<detail::json::array>()->emplace_back();
      if constexpr (std::is_floating_point_v<T>) {
        new_entry.data = static_cast<double>(x);
      } else {
        new_entry.data = static_cast<int64_t>(x);
      }
      return true;
    }
    default:
      fail(type::number);
      return false;
  }
}

// -- constructors, destructors, and assignment operators ----------------------

json_builder::~json_builder() {
  // nop
}

// -- modifiers ----------------------------------------------------------------

void json_builder::reset() {
  stack_.clear();
  if (!storage_) {
    storage_ = make_counted<detail::json::storage>();
  } else {
    storage_->buf.reclaim();
  }
  val_ = detail::json::make_value(storage_);
  push(val_, type::element);
}

json_value json_builder::seal() {
  return json_value{val_, std::move(storage_)};
}

// -- overrides ----------------------------------------------------------------

bool json_builder::begin_object(type_id_t id, std::string_view name) {
  auto add_type_annotation = [this, id, name] {
    CAF_ASSERT(top() == type::key);
    *top_ptr<key_type>() = "@type"sv;
    pop();
    CAF_ASSERT(top() == type::element);
    if (auto tname = query_type_name(id); !tname.empty()) {
      top_ptr()->data = tname;
    } else {
      top_ptr()->data = name;
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

bool json_builder::end_object() {
  return end_associative_array();
}

bool json_builder::begin_field(std::string_view name) {
  if (begin_key_value_pair()) {
    CAF_ASSERT(top() == type::key);
    *top_ptr<key_type>() = name;
    pop();
    CAF_ASSERT(top() == type::element);
    return true;
  } else {
    return false;
  }
}

bool json_builder::begin_field(std::string_view name, bool is_present) {
  if (skip_empty_fields_ && !is_present) {
    auto t = top();
    switch (t) {
      case type::object:
        push(static_cast<detail::json::member*>(nullptr));
        return true;
      default: {
        std::string str = "expected object, found ";
        str += as_json_type_name(t);
        emplace_error(sec::runtime_error, class_name, __func__, std::move(str));
        return false;
      }
    }
  } else if (begin_key_value_pair()) {
    CAF_ASSERT(top() == type::key);
    *top_ptr<key_type>() = name;
    pop();
    CAF_ASSERT(top() == type::element);
    if (!is_present) {
      // We don't need to assign nullptr explicitly since it's the default.
      pop();
    }
    return true;
  } else {
    return false;
  }
}

bool json_builder::begin_field(std::string_view name,
                               span<const type_id_t> types, size_t index) {
  if (index >= types.size()) {
    emplace_error(sec::runtime_error, "index >= types.size()");
    return false;
  }
  if (begin_key_value_pair()) {
    if (auto tname = query_type_name(types[index]); !tname.empty()) {
      auto& annotation = top_obj()->emplace_back();
      annotation.key = detail::json::concat({"@"sv, name, field_type_suffix_},
                                            &storage_->buf);
      annotation.val = detail::json::make_value(storage_);
      annotation.val->data = tname;
    } else {
      emplace_error(sec::runtime_error, "query_type_name failed");
      return false;
    }
    CAF_ASSERT(top() == type::key);
    *top_ptr<key_type>() = name;
    pop();
    CAF_ASSERT(top() == type::element);
    return true;
  } else {
    return false;
  }
}

bool json_builder::begin_field(std::string_view name, bool is_present,
                               span<const type_id_t> types, size_t index) {
  if (is_present)
    return begin_field(name, types, index);
  else
    return begin_field(name, is_present);
}

bool json_builder::end_field() {
  return end_key_value_pair();
}

bool json_builder::begin_tuple(size_t size) {
  return begin_sequence(size);
}

bool json_builder::end_tuple() {
  return end_sequence();
}

bool json_builder::begin_key_value_pair() {
  auto t = top();
  switch (t) {
    case type::object: {
      auto* obj = top_ptr<detail::json::object>();
      auto& new_member = obj->emplace_back();
      new_member.val = detail::json::make_value(storage_);
      push(&new_member);
      push(new_member.val, type::element);
      push(&new_member.key);
      return true;
    }
    default: {
      std::string str = "expected object, found ";
      str += as_json_type_name(t);
      emplace_error(sec::runtime_error, class_name, __func__, std::move(str));
      return false;
    }
  }
}

bool json_builder::end_key_value_pair() {
  return pop_if(type::member);
}

bool json_builder::begin_sequence(size_t) {
  switch (top()) {
    default:
      emplace_error(sec::runtime_error, "unexpected begin_sequence");
      return false;
    case type::element: {
      auto* val = top_ptr();
      val->assign_array(storage_);
      push(val, type::array);
      return true;
    }
    case type::array: {
      auto* arr = top_ptr<detail::json::array>();
      auto& new_val = arr->emplace_back();
      new_val.assign_array(storage_);
      push(&new_val, type::array);
      return true;
    }
  }
}

bool json_builder::end_sequence() {
  return pop_if(type::array);
}

bool json_builder::begin_associative_array(size_t) {
  switch (top()) {
    default:
      emplace_error(sec::runtime_error, class_name, __func__,
                    "unexpected begin_object or begin_associative_array");
      return false;
    case type::element:
      top_ptr()->assign_object(storage_);
      stack_.back().t = type::object;
      return true;
    case type::array: {
      auto& new_val = top_ptr<detail::json::array>()->emplace_back();
      new_val.assign_object(storage_);
      push(&new_val, type::object);
      return true;
    }
  }
}

bool json_builder::end_associative_array() {
  return pop_if(type::object);
}

bool json_builder::value(std::byte x) {
  return number(std::to_integer<uint8_t>(x));
}

bool json_builder::value(bool x) {
  switch (top()) {
    case type::element:
      top_ptr()->data = x;
      pop();
      return true;
    case type::key:
      *top_ptr<key_type>() = x ? "true"sv : "false"sv;
      return true;
    case type::array: {
      auto* arr = top_ptr<detail::json::array>();
      arr->emplace_back().data = x;
      return true;
    }
    default:
      fail(type::boolean);
      return false;
  }
}

bool json_builder::value(int8_t x) {
  return number(x);
}

bool json_builder::value(uint8_t x) {
  return number(x);
}

bool json_builder::value(int16_t x) {
  return number(x);
}

bool json_builder::value(uint16_t x) {
  return number(x);
}

bool json_builder::value(int32_t x) {
  return number(x);
}

bool json_builder::value(uint32_t x) {
  return number(x);
}

bool json_builder::value(int64_t x) {
  return number(x);
}

bool json_builder::value(uint64_t x) {
  return number(x);
}

bool json_builder::value(float x) {
  return number(x);
}

bool json_builder::value(double x) {
  return number(x);
}

bool json_builder::value(long double x) {
  return number(x);
}

bool json_builder::value(std::string_view x) {
  switch (top()) {
    case type::element:
      top_ptr()->assign_string(x, storage_);
      pop();
      return true;
    case type::key:
      *top_ptr<key_type>() = detail::json::realloc(x, storage_);
      pop();
      return true;
    case type::array: {
      auto& new_val = top_ptr<detail::json::array>()->emplace_back();
      new_val.assign_string(x, storage_);
      return true;
    }
    default:
      fail(type::string);
      return false;
  }
}

bool json_builder::value(const std::u16string&) {
  emplace_error(sec::unsupported_operation,
                "u16string not supported yet by caf::json_builder");
  return false;
}

bool json_builder::value(const std::u32string&) {
  emplace_error(sec::unsupported_operation,
                "u32string not supported yet by caf::json_builder");
  return false;
}

bool json_builder::value(span<const std::byte> x) {
  std::vector<char> buf;
  buf.reserve(x.size() * 2);
  detail::append_hex(buf, reinterpret_cast<const void*>(x.data()), x.size());
  auto hex_str = std::string_view{buf.data(), buf.size()};
  switch (top()) {
    case type::element:
      top_ptr()->assign_string(hex_str, storage_);
      pop();
      return true;
    case type::key:
      *top_ptr<key_type>() = detail::json::realloc(hex_str, storage_);
      pop();
      return true;
    case type::array: {
      auto& new_val = top_ptr<detail::json::array>()->emplace_back();
      new_val.assign_string(hex_str, storage_);
      return true;
    }
    default:
      fail(type::string);
      return false;
  }
}

// -- state management ---------------------------------------------------------

void json_builder::init() {
  has_human_readable_format_ = true;
  storage_ = make_counted<detail::json::storage>();
  val_ = detail::json::make_value(storage_);
  stack_.reserve(32);
  push(val_, type::element);
}

json_builder::type json_builder::top() {
  if (!stack_.empty())
    return stack_.back().t;
  else
    return type::null;
}

template <class T>
T* json_builder::top_ptr() {
  if constexpr (std::is_same_v<T, key_type>) {
    return stack_.back().key_ptr;
  } else if constexpr (std::is_same_v<T, detail::json::member>) {
    return stack_.back().mem_ptr;
  } else if constexpr (std::is_same_v<T, detail::json::object>) {
    return &std::get<detail::json::object>(stack_.back().val_ptr->data);
  } else if constexpr (std::is_same_v<T, detail::json::array>) {
    return &std::get<detail::json::array>(stack_.back().val_ptr->data);
  } else {
    static_assert(std::is_same_v<T, detail::json::value>);
    return stack_.back().val_ptr;
  }
}

detail::json::object* json_builder::top_obj() {
  auto is_obj = [](const entry& x) { return x.t == type::object; };
  auto i = std::find_if(stack_.rbegin(), stack_.rend(), is_obj);
  if (i != stack_.rend())
    return &std::get<detail::json::object>(i->val_ptr->data);
  CAF_RAISE_ERROR("json_builder::top_obj was unable to find an object");
}

void json_builder::push(detail::json::value* ptr, type t) {
  stack_.emplace_back(ptr, t);
}

void json_builder::push(detail::json::value::member* ptr) {
  stack_.emplace_back(ptr);
}

void json_builder::push(key_type* ptr) {
  stack_.emplace_back(ptr);
}

// Backs up one level of nesting.
bool json_builder::pop() {
  if (!stack_.empty()) {
    stack_.pop_back();
    return true;
  } else {
    std::string str = "pop() called with an empty stack: begin/end mismatch";
    emplace_error(sec::runtime_error, std::move(str));
    return false;
  }
}

bool json_builder::pop_if(type t) {
  if (!stack_.empty() && stack_.back().t == t) {
    stack_.pop_back();
    return true;
  } else {
    std::string str = "pop_if failed: expected ";
    str += as_json_type_name(t);
    if (stack_.empty()) {
      str += ", found an empty stack";
    } else {
      str += ", found ";
      str += as_json_type_name(stack_.back().t);
    }
    emplace_error(sec::runtime_error, std::move(str));
    return false;
  }
}

void json_builder::fail(type t) {
  std::string str = "failed to write a ";
  str += as_json_type_name(t);
  str += ": invalid position (begin/end mismatch?)";
  emplace_error(sec::runtime_error, std::move(str));
}

bool json_builder::inside_object() const noexcept {
  auto is_object = [](const entry& x) { return x.t == type::object; };
  return std::any_of(stack_.begin(), stack_.end(), is_object);
}

} // namespace caf
