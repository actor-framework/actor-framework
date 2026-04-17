// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/stringification_inspector.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/print.hpp"
#include "caf/internal/stringification_inspector_node.hpp"

#include <algorithm>
#include <ctime>

namespace caf::detail {

namespace {

class stringification_inspector_impl final : public serializer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit stringification_inspector_impl(std::string& result)
    : result_(result) {
    stack_.reserve(32);
  }

  stringification_inspector_impl(const stringification_inspector_impl&)
    = delete;

  stringification_inspector_impl&
  operator=(const stringification_inspector_impl&)
    = delete;

  // -- properties -------------------------------------------------------------

  [[nodiscard]] bool has_human_readable_format() const noexcept override {
    return true;
  }

  // -- serializer interface ---------------------------------------------------

  void set_error(error stop_reason) override {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept override {
    return err_;
  }

  bool begin_object(type_id_t, std::string_view name) override {
    sep();
    if (name != "std::string") {
      result_.insert(result_.end(), name.begin(), name.end());
      result_ += '(';
    } else {
      in_string_object_ = true;
    }
    push(internal::stringification_inspector_node::object);
    return true;
  }

  bool end_object() override {
    if (!in_string_object_)
      result_ += ')';
    else
      in_string_object_ = false;
    return pop_if(internal::stringification_inspector_node::object);
  }

  bool begin_field(std::string_view) override {
    return true;
  }

  bool begin_field(std::string_view, bool is_present) override {
    sep();
    if (!is_present)
      result_ += "null";
    else
      result_ += '*';
    return true;
  }

  bool begin_field(std::string_view, std::span<const type_id_t>,
                   size_t) override {
    return true;
  }

  bool begin_field(std::string_view, bool is_present,
                   std::span<const type_id_t>, size_t) override {
    sep();
    if (!is_present)
      result_ += "null";
    else
      result_ += '*';
    return true;
  }

  bool end_field() override {
    return true;
  }

  bool begin_tuple(size_t size) override {
    return begin_sequence(size);
  }

  bool end_tuple() override {
    return end_sequence();
  }

  bool begin_key_value_pair() override {
    switch (top()) {
      case internal::stringification_inspector_node::sequence:
        return begin_tuple(2);
      case internal::stringification_inspector_node::map:
        push(internal::stringification_inspector_node::member);
        return true;
      default:
        return false;
    }
  }

  bool end_key_value_pair() override {
    switch (top()) {
      case internal::stringification_inspector_node::sequence:
        return end_tuple();
      case internal::stringification_inspector_node::member:
        return pop_if(internal::stringification_inspector_node::member);
      default:
        return false;
    }
  }

  bool begin_sequence(size_t) override {
    sep();
    result_ += '[';
    push(internal::stringification_inspector_node::sequence);
    return true;
  }

  bool end_sequence() override {
    if (pop_if(internal::stringification_inspector_node::sequence)) {
      result_ += ']';
      return true;
    }
    return true;
  }

  bool begin_associative_array(size_t) override {
    sep();
    result_ += '{';
    push(internal::stringification_inspector_node::map);
    return true;
  }

  bool end_associative_array() override {
    result_ += '}';
    return pop_if(internal::stringification_inspector_node::map);
  }

  bool value(std::byte x) override {
    return value(const_byte_span(&x, 1));
  }

  bool value(bool x) override {
    sep();
    result_ += x ? "true" : "false";
    return true;
  }

  bool value(int8_t x) override {
    return int_value(static_cast<int64_t>(x));
  }

  bool value(uint8_t x) override {
    return int_value(static_cast<uint64_t>(x));
  }

  bool value(int16_t x) override {
    return int_value(static_cast<int64_t>(x));
  }

  bool value(uint16_t x) override {
    return int_value(static_cast<uint64_t>(x));
  }

  bool value(int32_t x) override {
    return int_value(static_cast<int64_t>(x));
  }

  bool value(uint32_t x) override {
    return int_value(static_cast<uint64_t>(x));
  }

  bool value(int64_t x) override {
    return int_value(x);
  }

  bool value(uint64_t x) override {
    return int_value(x);
  }

  bool value(float x) override {
    sep();
    auto str = std::to_string(x);
    result_ += str;
    return true;
  }

  bool value(double x) override {
    sep();
    detail::print(result_, x);
    return true;
  }

  bool value(long double x) override {
    sep();
    detail::print(result_, x);
    return true;
  }

  bool value(std::string_view x) override {
    sep();
    if (x.empty()) {
      result_ += R"("")";
      return true;
    }
    if (x[0] == '"') {
      result_.insert(result_.end(), x.begin(), x.end());
      return true;
    }
    detail::print_escaped(result_, x);
    return true;
  }

  bool value(const std::u16string&) override {
    sep();
    result_ += "<unprintable>";
    return true;
  }

  bool value(const std::u32string&) override {
    sep();
    result_ += "<unprintable>";
    return true;
  }

  bool value(const_byte_span x) override {
    sep();
    detail::append_hex(result_, x.data(), x.size());
    return true;
  }

  caf::actor_handle_codec* actor_handle_codec() override {
    return nullptr;
  }

  // -- stringification_inspector extensions -----------------------------------

  bool stringify_void_ptr(const void* x) {
    sep();
    if (x == nullptr) {
      result_ += "null";
      return true;
    }
    result_ += "*<";
    auto addr = reinterpret_cast<intptr_t>(x);
    result_ += std::to_string(addr);
    result_ += ">";
    return true;
  }

  void append(std::string_view str) {
    sep();
    result_.insert(result_.end(), str.begin(), str.end());
  }

  static stringification_inspector_impl& downcast(serializer& ptr) noexcept {
    return static_cast<stringification_inspector_impl&>(ptr);
  }

private:
  bool int_value(int64_t x) {
    sep();
    detail::print(result_, x);
    return true;
  }

  bool int_value(uint64_t x) {
    sep();
    detail::print(result_, x);
    return true;
  }

  internal::stringification_inspector_node top() {
    if (!stack_.empty())
      return stack_.back().t;
    else
      return internal::stringification_inspector_node::null;
  }

  void push(internal::stringification_inspector_node t) {
    auto tmp = entry{t, true};
    stack_.push_back(tmp);
  }

  bool pop_if(internal::stringification_inspector_node t) {
    if (!stack_.empty() && stack_.back() == t) {
      stack_.pop_back();
      return true;
    }
    if (stack_.empty()) {
      err_ = make_error(sec::runtime_error,
                        "pop_if failed: expected {} but found an empty stack",
                        as_stringification_type_name(t));
    } else {
      err_ = make_error(sec::runtime_error,
                        "pop_if failed: expected {} but found {}",
                        as_stringification_type_name(t),
                        as_stringification_type_name(stack_.back().t));
    }
    return false;
  }

  struct entry {
    internal::stringification_inspector_node t;
    bool fill;
    friend bool
    operator==(entry x, internal::stringification_inspector_node y) noexcept {
      return x.t == y;
    };
  };

  void sep() {
    if (result_.empty())
      return;
    switch (result_.back()) {
      case '(':
      case '[':
      case '*':
      case ' ':
        return;
      case '{':
        CAF_ASSERT(!stack_.empty());
        stack_.back().fill = false;
        return;
    }
    const auto t = top();
    if (t == internal::stringification_inspector_node::member
        && stack_.back().fill) {
      result_ += ", ";
      stack_.back().fill = false;
    } else if (t == internal::stringification_inspector_node::member
               && !stack_.back().fill) {
      result_ += " = ";
    } else
      result_ += ", ";
  }

  std::vector<entry> stack_;

  std::string& result_;

  bool in_string_object_ = false;

  error err_;
};

} // namespace

// -- constructors, destructors, and assignment operators --------------------

stringification_inspector::stringification_inspector(std::string& result)
  : super(new(impl_storage_) stringification_inspector_impl(result)) {
  static_assert(sizeof(stringification_inspector_impl) <= impl_storage_size);
}

stringification_inspector::~stringification_inspector() noexcept {
  // nop
}

// -- extensions -------------------------------------------------------------

bool stringification_inspector::value(const void* x) {
  return stringification_inspector_impl::downcast(*impl_).stringify_void_ptr(x);
}

void stringification_inspector::append(std::string_view str) {
  stringification_inspector_impl::downcast(*impl_).append(str);
}

} // namespace caf::detail
