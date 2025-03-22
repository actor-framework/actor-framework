// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/stringification_inspector.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/detail/print.hpp"
#include "caf/internal/fast_pimpl.hpp"
#include "caf/internal/stringification_inspector_node.hpp"

#include <algorithm>
#include <ctime>

namespace caf::detail {

namespace {

class impl : public save_inspector_base<impl>,
             public internal::fast_pimpl<impl> {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector_base<impl>;

  // -- constructors, destructors, and assignment operators --------------------

  impl(std::string& result) : result_(result) {
    stack_.reserve(32);
  }

  // -- properties -------------------------------------------------------------

  constexpr bool has_human_readable_format() const noexcept {
    return true;
  }

  // -- serializer interface ---------------------------------------------------

  void set_error(error stop_reason) override {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept override {
    return err_;
  }

  bool begin_object(type_id_t, std::string_view name) {
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

  bool end_object() {
    if (!in_string_object_)
      result_ += ')';
    else
      in_string_object_ = false;
    return pop_if(internal::stringification_inspector_node::object);
  }

  bool begin_field(std::string_view) {
    return true;
  }

  bool begin_field(std::string_view, bool is_present) {
    sep();
    if (!is_present)
      result_ += "null";
    else
      result_ += '*';
    return true;
  }

  bool begin_field(std::string_view, span<const type_id_t>, size_t) {
    return true;
  }

  bool begin_field(std::string_view, bool is_present, span<const type_id_t>,
                   size_t) {
    sep();
    if (!is_present)
      result_ += "null";
    else
      result_ += '*';
    return true;
  }

  bool end_field() {
    return true;
  }

  bool begin_tuple(size_t size) {
    return begin_sequence(size);
  }

  bool end_tuple() {
    return end_sequence();
  }

  bool begin_key_value_pair() {
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

  bool end_key_value_pair() {
    switch (top()) {
      case internal::stringification_inspector_node::sequence:
        return end_tuple();
      case internal::stringification_inspector_node::member:
        return pop_if(internal::stringification_inspector_node::member);
      default:
        return false;
    }
  }

  bool begin_sequence(size_t) {
    sep();
    result_ += '[';
    push(internal::stringification_inspector_node::sequence);
    return true;
  }

  bool end_sequence() {
    if (pop_if(internal::stringification_inspector_node::sequence)) {
      result_ += ']';
      return true;
    }
    return true;
  }

  bool begin_associative_array(size_t) {
    sep();
    result_ += '{';
    push(internal::stringification_inspector_node::map);
    return true;
  }

  bool end_associative_array() {
    result_ += '}';
    return pop_if(internal::stringification_inspector_node::map);
  }

  bool value(std::byte x) {
    return value(const_byte_span(&x, 1));
  }

  bool value(bool x) {
    sep();
    result_ += x ? "true" : "false";
    return true;
  }

  bool value(float x) {
    sep();
    auto str = std::to_string(x);
    result_ += str;
    return true;
  }

  bool value(double x) {
    sep();
    detail::print(result_, x);
    return true;
  }

  bool value(long double x) {
    sep();
    detail::print(result_, x);
    return true;
  }

  bool value(timespan x) {
    sep();
    detail::print(result_, x);
    return true;
  }

  bool value(timestamp x) {
    sep();
    append_timestamp_to_string(result_, x);
    return true;
  }

  bool value(std::string_view x) {
    sep();
    if (x.empty()) {
      result_ += R"("")";
      return true;
    }
    if (x[0] == '"') {
      // Assume an already escaped string.
      result_.insert(result_.end(), x.begin(), x.end());
      return true;
    }
    detail::print_escaped(result_, x);
    return true;
  }

  bool value(void* x) {
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

  bool value(const std::u16string&) {
    sep();
    // Convert to UTF-8 and print?
    result_ += "<unprintable>";
    return true;
  }

  bool value(const std::u32string&) {
    sep();
    // Convert to UTF-8 and print?
    result_ += "<unprintable>";
    return true;
  }

  bool value(const_byte_span x) {
    sep();
    detail::append_hex(result_, x.data(), x.size());
    return true;
  }

  bool value(const strong_actor_ptr& ptr) {
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

  bool value(const weak_actor_ptr& ptr) {
    return value(ptr.lock());
  }

  using super::list;

  bool list(const std::vector<bool>& xs) {
    begin_sequence(xs.size());
    for (bool x : xs)
      value(x);
    return end_sequence();
  }

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

  void append(std::string_view str) {
    sep();
    result_.insert(result_.end(), str.begin(), str.end());
  }

  // Returns the current top of the stack or `null` if empty.
  internal::stringification_inspector_node top() {
    if (!stack_.empty())
      return stack_.back().t;
    else
      return internal::stringification_inspector_node::null;
  }

  // Enters a new level of nesting.
  void push(internal::stringification_inspector_node t) {
    auto tmp = entry{t, true};
    stack_.push_back(tmp);
  }

  // Backs up one level of nesting.
  bool pop() {
    if (!stack_.empty()) {
      stack_.pop_back();
      return true;
    }
    err_ = make_error(sec::runtime_error,
                      "pop() called with an empty stack: begin/end mismatch");
    return false;
  }

  // Backs up one level of nesting but checks that current top is `t` before.
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

private:
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
      case ' ': // only at back if we've printed ", " before
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

  // Bookkeeping for where we are in the current object.
  std::vector<entry> stack_;

  std::string& result_;

  bool in_string_object_ = false;

  error err_;
};

} // namespace

// -- constructors, destructors, and assignment operators --------------------

stringification_inspector::stringification_inspector(std::string& result) {
  impl::construct(impl_, result);
}

stringification_inspector::~stringification_inspector() {
  impl::destruct(impl_);
}

// -- properties -------------------------------------------------------------

bool stringification_inspector::has_human_readable_format() const noexcept {
  return impl::cast(impl_).has_human_readable_format();
}

// -- serializer interface ---------------------------------------------------

void stringification_inspector::set_error(error stop_reason) {
  impl::cast(impl_).set_error(std::move(stop_reason));
}

error& stringification_inspector::get_error() noexcept {
  return impl::cast(impl_).get_error();
}

bool stringification_inspector::begin_object(type_id_t type,
                                             std::string_view name) {
  return impl::cast(impl_).begin_object(type, name);
}

bool stringification_inspector::end_object() {
  return impl::cast(impl_).end_object();
}

bool stringification_inspector::begin_field(std::string_view name) {
  return impl::cast(impl_).begin_field(name);
}

bool stringification_inspector::begin_field(std::string_view name,
                                            bool is_present) {
  return impl::cast(impl_).begin_field(name, is_present);
}

bool stringification_inspector::begin_field(std::string_view name,
                                            span<const type_id_t> types,
                                            size_t size) {
  return impl::cast(impl_).begin_field(name, types, size);
}

bool stringification_inspector::begin_field(std::string_view name,
                                            bool is_present,
                                            span<const type_id_t> types,
                                            size_t size) {
  return impl::cast(impl_).begin_field(name, is_present, types, size);
}

bool stringification_inspector::end_field() {
  return impl::cast(impl_).end_field();
}

bool stringification_inspector::begin_tuple(size_t size) {
  return impl::cast(impl_).begin_tuple(size);
}

bool stringification_inspector::end_tuple() {
  return impl::cast(impl_).end_tuple();
}

bool stringification_inspector::begin_key_value_pair() {
  return impl::cast(impl_).begin_key_value_pair();
}

bool stringification_inspector::end_key_value_pair() {
  return impl::cast(impl_).end_key_value_pair();
}

bool stringification_inspector::begin_sequence(size_t size) {
  return impl::cast(impl_).begin_sequence(size);
}

bool stringification_inspector::end_sequence() {
  return impl::cast(impl_).end_sequence();
}

bool stringification_inspector::begin_associative_array(size_t size) {
  return impl::cast(impl_).begin_associative_array(size);
}

bool stringification_inspector::end_associative_array() {
  return impl::cast(impl_).end_associative_array();
}

bool stringification_inspector::value(std::byte x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(bool x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(float x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(double x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(long double x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(timespan x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(timestamp x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(std::string_view str) {
  return impl::cast(impl_).value(str);
}

bool stringification_inspector::value(void* x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(const std::u16string& x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(const std::u32string& x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(const_byte_span x) {
  return impl::cast(impl_).value(x);
}

bool stringification_inspector::value(const strong_actor_ptr& ptr) {
  return impl::cast(impl_).value(ptr);
}

bool stringification_inspector::value(const weak_actor_ptr& ptr) {
  return impl::cast(impl_).value(ptr);
}

bool stringification_inspector::list(const std::vector<bool>& xs) {
  return impl::cast(impl_).list(xs);
}

void stringification_inspector::append(std::string_view str) {
  impl::cast(impl_).append(str);
}

bool stringification_inspector::int_value(int64_t x) {
  return impl::cast(impl_).int_value(x);
}

bool stringification_inspector::int_value(uint64_t x) {
  return impl::cast(impl_).int_value(x);
}

} // namespace caf::detail
