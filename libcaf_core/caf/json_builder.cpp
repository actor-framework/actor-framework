// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_builder.hpp"

#include "caf/detail/append_hex.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/print.hpp"
#include "caf/format_to_error.hpp"
#include "caf/internal/fast_pimpl.hpp"
#include "caf/internal/json_node.hpp"
#include "caf/json_value.hpp"

#include <type_traits>

using namespace std::literals;

namespace caf {

namespace {

/// The default value for `skip_empty_fields()`.
constexpr bool skip_empty_fields_default = true;

/// The default value for `skip_object_type_annotation()`.
constexpr bool skip_object_type_annotation_default = false;

/// The value value for `field_type_suffix()`.
constexpr std::string_view field_type_suffix_default = "-type";

static constexpr const char class_name[] = "caf::json_builder";

class impl : public serializer, public internal::fast_pimpl<impl> {
public:
  // -- member types -----------------------------------------------------------

  using super = serializer;

  // -- constructors, destructors, and assignment operators --------------------

  impl() {
    init();
  }

  impl(const impl&) = delete;

  impl& operator=(const impl&) = delete;

  // -- properties -------------------------------------------------------------

  [[nodiscard]] bool skip_empty_fields() const noexcept {
    return skip_empty_fields_;
  }

  void skip_empty_fields(bool value) noexcept {
    skip_empty_fields_ = value;
  }

  [[nodiscard]] bool skip_object_type_annotation() const noexcept {
    return skip_object_type_annotation_;
  }

  void skip_object_type_annotation(bool value) noexcept {
    skip_object_type_annotation_ = value;
  }

  [[nodiscard]] std::string_view field_type_suffix() const noexcept {
    return field_type_suffix_;
  }

  void field_type_suffix(std::string_view suffix) noexcept {
    field_type_suffix_ = suffix;
  }

  // -- modifiers --------------------------------------------------------------

  void reset() {
    stack_.clear();
    if (!storage_) {
      storage_ = make_counted<detail::json::storage>();
    } else {
      storage_->buf.reclaim();
    }
    val_ = detail::json::make_value(storage_);
    push(val_, internal::json_node::element);
  }

  json_value seal() {
    return json_value{val_, std::move(storage_)};
  }

  // -- overrides --------------------------------------------------------------

  void set_error(error stop_reason) override {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept override {
    return err_;
  }

  caf::actor_system* sys() const noexcept override {
    return sys_;
  }

  bool has_human_readable_format() const noexcept override {
    return true;
  }

  bool begin_object(type_id_t id, std::string_view name) override {
    auto add_type_annotation = [this, id, name] {
      CAF_ASSERT(top() == internal::json_node::key);
      *top_ptr<key_type>() = "@type"sv;
      pop();
      CAF_ASSERT(top() == internal::json_node::element);
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

  bool end_object() override {
    return end_associative_array();
  }

  bool begin_field(std::string_view name) override {
    if (begin_key_value_pair()) {
      CAF_ASSERT(top() == internal::json_node::key);
      *top_ptr<key_type>() = name;
      pop();
      CAF_ASSERT(top() == internal::json_node::element);
      return true;
    } else {
      return false;
    }
  }

  bool begin_field(std::string_view name, bool is_present) override {
    if (skip_empty_fields_ && !is_present) {
      auto t = top();
      switch (t) {
        case internal::json_node::object:
          push(static_cast<detail::json::member*>(nullptr));
          return true;
        default: {
          err_ = format_to_error(sec::runtime_error,
                                 "{}::{}: expected object, found {}",
                                 class_name, __func__, as_json_type_name(t));
          return false;
        }
      }
    } else if (begin_key_value_pair()) {
      CAF_ASSERT(top() == internal::json_node::key);
      *top_ptr<key_type>() = name;
      pop();
      CAF_ASSERT(top() == internal::json_node::element);
      if (!is_present) {
        // We don't need to assign nullptr explicitly since it's the default.
        pop();
      }
      return true;
    } else {
      return false;
    }
  }

  bool begin_field(std::string_view name, span<const type_id_t> types,
                   size_t index) override {
    if (index >= types.size()) {
      err_ = make_error(sec::runtime_error, "index >= types.size()");
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
        err_ = make_error(sec::runtime_error, "query_type_name failed");
        return false;
      }
      CAF_ASSERT(top() == internal::json_node::key);
      *top_ptr<key_type>() = name;
      pop();
      CAF_ASSERT(top() == internal::json_node::element);
      return true;
    } else {
      return false;
    }
  }

  bool begin_field(std::string_view name, bool is_present,
                   span<const type_id_t> types, size_t index) override {
    if (is_present)
      return begin_field(name, types, index);
    else
      return begin_field(name, is_present);
  }

  bool end_field() override {
    return end_key_value_pair();
  }

  bool begin_tuple(size_t size) override {
    return begin_sequence(size);
  }

  bool end_tuple() override {
    return end_sequence();
  }

  bool begin_key_value_pair() override {
    auto t = top();
    switch (t) {
      case internal::json_node::object: {
        auto* obj = top_ptr<detail::json::object>();
        auto& new_member = obj->emplace_back();
        new_member.val = detail::json::make_value(storage_);
        push(&new_member);
        push(new_member.val, internal::json_node::element);
        push(&new_member.key);
        return true;
      }
      default: {
        err_ = format_to_error(sec::runtime_error,
                               "{}::{}: expected object, found {}", class_name,
                               __func__, as_json_type_name(t));
        return false;
      }
    }
  }

  bool end_key_value_pair() override {
    return pop_if(internal::json_node::member);
  }

  bool begin_sequence(size_t) override {
    switch (top()) {
      default:
        err_ = make_error(sec::runtime_error, "unexpected begin_sequence");
        return false;
      case internal::json_node::element: {
        top_ptr()->assign_array(storage_);
        stack_.back().t = internal::json_node::array;
        return true;
      }
      case internal::json_node::array: {
        auto* arr = top_ptr<detail::json::array>();
        auto& new_val = arr->emplace_back();
        new_val.assign_array(storage_);
        push(&new_val, internal::json_node::array);
        return true;
      }
    }
  }

  bool end_sequence() override {
    return pop_if(internal::json_node::array);
  }

  bool begin_associative_array(size_t) override {
    switch (top()) {
      default:
        err_ = make_error(sec::runtime_error, "{}::{}: {}", class_name,
                          __func__,
                          "unexpected begin_object or begin_associative_array");
        return false;
      case internal::json_node::element:
        top_ptr()->assign_object(storage_);
        stack_.back().t = internal::json_node::object;
        return true;
      case internal::json_node::array: {
        auto& new_val = top_ptr<detail::json::array>()->emplace_back();
        new_val.assign_object(storage_);
        push(&new_val, internal::json_node::object);
        return true;
      }
    }
  }

  bool end_associative_array() override {
    return pop_if(internal::json_node::object);
  }

  bool value(std::byte x) override {
    return number(std::to_integer<uint8_t>(x));
  }

  bool value(bool x) override {
    switch (top()) {
      case internal::json_node::element:
        top_ptr()->data = x;
        pop();
        return true;
      case internal::json_node::key:
        *top_ptr<key_type>() = x ? "true"sv : "false"sv;
        pop();
        return true;
      case internal::json_node::array: {
        auto* arr = top_ptr<detail::json::array>();
        arr->emplace_back().data = x;
        return true;
      }
      default:
        fail(internal::json_node::boolean);
        return false;
    }
  }

  bool value(int8_t x) override {
    return number(x);
  }

  bool value(uint8_t x) override {
    return number(x);
  }

  bool value(int16_t x) override {
    return number(x);
  }

  bool value(uint16_t x) override {
    return number(x);
  }

  bool value(int32_t x) override {
    return number(x);
  }

  bool value(uint32_t x) override {
    return number(x);
  }

  bool value(int64_t x) override {
    return number(x);
  }

  bool value(uint64_t x) override {
    return number(x);
  }

  bool value(float x) override {
    return number(x);
  }

  bool value(double x) override {
    return number(x);
  }

  bool value(long double x) override {
    return number(x);
  }

  bool value(std::string_view x) override {
    switch (top()) {
      case internal::json_node::element:
        top_ptr()->assign_string(x, storage_);
        pop();
        return true;
      case internal::json_node::key:
        *top_ptr<key_type>() = detail::json::realloc(x, storage_);
        pop();
        return true;
      case internal::json_node::array: {
        auto& new_val = top_ptr<detail::json::array>()->emplace_back();
        new_val.assign_string(x, storage_);
        return true;
      }
      default:
        fail(internal::json_node::string);
        return false;
    }
  }

  bool value(const std::u16string&) override {
    err_ = make_error(sec::unsupported_operation,
                      "u16string not supported yet by caf::json_builder");
    return false;
  }

  bool value(const std::u32string&) override {
    err_ = make_error(sec::unsupported_operation,
                      "u32string not supported yet by caf::json_builder");
    return false;
  }

  bool value(span<const std::byte> x) override {
    std::vector<char> buf;
    buf.reserve(x.size() * 2);
    detail::append_hex(buf, reinterpret_cast<const void*>(x.data()), x.size());
    auto hex_str = std::string_view{buf.data(), buf.size()};
    switch (top()) {
      case internal::json_node::element:
        top_ptr()->assign_string(hex_str, storage_);
        pop();
        return true;
      case internal::json_node::key:
        *top_ptr<key_type>() = detail::json::realloc(hex_str, storage_);
        pop();
        return true;
      case internal::json_node::array: {
        auto& new_val = top_ptr<detail::json::array>()->emplace_back();
        new_val.assign_string(hex_str, storage_);
        return true;
      }
      default:
        fail(internal::json_node::string);
        return false;
    }
  }

private:
  template <class T>
  bool number(T x) {
    switch (top()) {
      case internal::json_node::element: {
        if constexpr (std::is_floating_point_v<T>) {
          top_ptr()->data = static_cast<double>(x);
        } else {
          top_ptr()->data = static_cast<int64_t>(x);
        }
        pop();
        return true;
      }
      case internal::json_node::key: {
        std::string str;
        detail::print(str, x);
        *top_ptr<key_type>() = detail::json::realloc(str, &storage_->buf);
        pop();
        return true;
      }
      case internal::json_node::array: {
        auto& new_entry = top_ptr<detail::json::array>()->emplace_back();
        if constexpr (std::is_floating_point_v<T>) {
          new_entry.data = static_cast<double>(x);
        } else {
          new_entry.data = static_cast<int64_t>(x);
        }
        return true;
      }
      default:
        fail(internal::json_node::number);
        return false;
    }
  }

  using key_type = std::string_view;

  // -- state management -------------------------------------------------------

  void init() {
    storage_ = make_counted<detail::json::storage>();
    val_ = detail::json::make_value(storage_);
    stack_.reserve(32);
    push(val_, internal::json_node::element);
  }

  // Returns the current top of the stack or `null` if empty.
  internal::json_node top() {
    if (!stack_.empty())
      return stack_.back().t;
    else
      return internal::json_node::null;
  }

  // Returns the current top of the stack or `null` if empty.
  template <class T = detail::json::value>
  T* top_ptr() {
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

  // Returns the current top-level object.
  detail::json::object* top_obj() {
    auto is_obj = [](const entry& x) {
      return x.t == internal::json_node::object;
    };
    auto i = std::find_if(stack_.rbegin(), stack_.rend(), is_obj);
    if (i != stack_.rend())
      return &std::get<detail::json::object>(i->val_ptr->data);
    CAF_RAISE_ERROR("json_builder::top_obj was unable to find an object");
  }

  // Enters a new level of nesting.
  void push(detail::json::value* ptr, internal::json_node t) {
    stack_.emplace_back(ptr, t);
  }

  // Enters a new level of nesting with type member.
  void push(detail::json::value::member* ptr) {
    stack_.emplace_back(ptr);
  }

  // Enters a new level of nesting with type key.
  void push(key_type* ptr) {
    stack_.emplace_back(ptr);
  }

  // Backs up one level of nesting.
  bool pop() {
    if (!stack_.empty()) {
      stack_.pop_back();
      return true;
    } else {
      err_ = make_error(sec::runtime_error,
                        "pop() called with an empty stack: begin/end mismatch");
      return false;
    }
  }

  // Backs up one level of nesting but checks that current top is `t` before.
  bool pop_if(internal::json_node t) {
    if (!stack_.empty() && stack_.back().t == t) {
      stack_.pop_back();
      return true;
    }
    if (stack_.empty()) {
      err_ = format_to_error(sec::runtime_error,
                             "pop_if failed: expected {}, found an empty stack",
                             as_json_type_name(t));
    } else {
      err_ = format_to_error(sec::runtime_error,
                             "pop_if failed: expected {}, found {}",
                             as_json_type_name(t),
                             as_json_type_name(stack_.back().t));
    }
    return false;
  }

  // Sets an error reason that the inspector failed to write a t.
  void fail(internal::json_node t) {
    err_ = format_to_error(sec::runtime_error,
                           "failed to write a value of type {}: "
                           "invalid position (begin/end mismatch?)",
                           as_json_type_name(t));
  }

  // Checks whether any element in the stack has the type `object`.
  bool inside_object() const noexcept {
    auto is_object = [](const entry& x) {
      return x.t == internal::json_node::object;
    };
    return std::any_of(stack_.begin(), stack_.end(), is_object);
  }

  // -- member variables -------------------------------------------------------

  /// The actor system associated with this builder.
  actor_system* sys_ = nullptr;

  // Our output.
  detail::json::value* val_;

  // Storage for the assembled output.
  detail::json::storage_ptr storage_;

  struct entry {
    union {
      detail::json::value* val_ptr;
      detail::json::member* mem_ptr;
      key_type* key_ptr;
    };
    internal::json_node t;

    entry(detail::json::value* ptr, internal::json_node ptr_type) noexcept {
      val_ptr = ptr;
      t = ptr_type;
    }

    explicit entry(detail::json::member* ptr) noexcept {
      mem_ptr = ptr;
      t = internal::json_node::member;
    }

    explicit entry(key_type* ptr) noexcept {
      key_ptr = ptr;
      t = internal::json_node::key;
    }

    entry(const entry&) noexcept = default;

    entry& operator=(const entry&) noexcept = default;
  };

  // Bookkeeping for where we are in the current object.
  std::vector<entry> stack_;

  // Configures whether we omit empty fields entirely (true) or render empty
  // fields as `$field: null` (false).
  bool skip_empty_fields_ = skip_empty_fields_default;

  // Configures whether we omit the top-level `@type` annotation.
  bool skip_object_type_annotation_ = skip_object_type_annotation_default;

  // Configures the suffix for generating type annotations.
  std::string_view field_type_suffix_ = field_type_suffix_default;

  error err_;
};

} // namespace

json_builder::json_builder() {
  impl::construct(impl_);
}

json_builder::~json_builder() {
  impl::destruct(impl_);
}

// -- modifiers ----------------------------------------------------------------

void json_builder::reset() {
  impl::cast(impl_).reset();
}

json_value json_builder::seal() {
  return impl::cast(impl_).seal();
}

// -- properties -------------------------------------------------------------

bool json_builder::skip_empty_fields() const noexcept {
  return impl::cast(impl_).skip_empty_fields();
}

void json_builder::skip_empty_fields(bool value) noexcept {
  impl::cast(impl_).skip_empty_fields(value);
}

bool json_builder::skip_object_type_annotation() const noexcept {
  return impl::cast(impl_).skip_object_type_annotation();
}

void json_builder::skip_object_type_annotation(bool value) noexcept {
  impl::cast(impl_).skip_object_type_annotation(value);
}

std::string_view json_builder::field_type_suffix() const noexcept {
  return impl::cast(impl_).field_type_suffix();
}

void json_builder::field_type_suffix(std::string_view suffix) noexcept {
  impl::cast(impl_).field_type_suffix(suffix);
}

// -- overrides ----------------------------------------------------------------

void json_builder::set_error(error stop_reason) {
  impl::cast(impl_).set_error(std::move(stop_reason));
}

error& json_builder::get_error() noexcept {
  return impl::cast(impl_).get_error();
}

caf::actor_system* json_builder::sys() const noexcept {
  return impl::cast(impl_).sys();
}

bool json_builder::has_human_readable_format() const noexcept {
  return impl::cast(impl_).has_human_readable_format();
}

bool json_builder::begin_object(type_id_t id, std::string_view name) {
  return impl::cast(impl_).begin_object(id, name);
}

bool json_builder::end_object() {
  return impl::cast(impl_).end_object();
}

bool json_builder::begin_field(std::string_view name) {
  return impl::cast(impl_).begin_field(name);
}

bool json_builder::begin_field(std::string_view name, bool is_present) {
  return impl::cast(impl_).begin_field(name, is_present);
}

bool json_builder::begin_field(std::string_view name,
                               span<const type_id_t> types, size_t index) {
  return impl::cast(impl_).begin_field(name, types, index);
}

bool json_builder::begin_field(std::string_view name, bool is_present,
                               span<const type_id_t> types, size_t index) {
  return impl::cast(impl_).begin_field(name, is_present, types, index);
}

bool json_builder::end_field() {
  return impl::cast(impl_).end_field();
}

bool json_builder::begin_tuple(size_t size) {
  return impl::cast(impl_).begin_tuple(size);
}

bool json_builder::end_tuple() {
  return impl::cast(impl_).end_tuple();
}

bool json_builder::begin_key_value_pair() {
  return impl::cast(impl_).begin_key_value_pair();
}

bool json_builder::end_key_value_pair() {
  return impl::cast(impl_).end_key_value_pair();
}

bool json_builder::begin_sequence(size_t size) {
  return impl::cast(impl_).begin_sequence(size);
}

bool json_builder::end_sequence() {
  return impl::cast(impl_).end_sequence();
}

bool json_builder::begin_associative_array(size_t size) {
  return impl::cast(impl_).begin_associative_array(size);
}

bool json_builder::end_associative_array() {
  return impl::cast(impl_).end_associative_array();
}

bool json_builder::value(std::byte x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(bool x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(int8_t x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(uint8_t x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(int16_t x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(uint16_t x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(int32_t x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(uint32_t x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(int64_t x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(uint64_t x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(float x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(double x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(long double x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(std::string_view x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(const std::u16string& x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(const std::u32string& x) {
  return impl::cast(impl_).value(x);
}

bool json_builder::value(span<const std::byte> x) {
  return impl::cast(impl_).value(x);
}

} // namespace caf
