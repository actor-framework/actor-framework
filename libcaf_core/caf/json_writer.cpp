// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_writer.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/print.hpp"
#include "caf/format_to_error.hpp"
#include "caf/internal/fast_pimpl.hpp"
#include "caf/internal/json_node.hpp"
#include "caf/serializer.hpp"

namespace caf {

namespace {

/// The default value for `skip_empty_fields()`.
constexpr bool skip_empty_fields_default = true;

/// The default value for `skip_object_type_annotation()`.
constexpr bool skip_object_type_annotation_default = false;

/// The value value for `field_type_suffix()`.
constexpr std::string_view field_type_suffix_default = "-type";

static constexpr const char class_name[] = "caf::json_writer";

char last_non_ws_char(const std::vector<char>& buf) {
  auto not_ws = [](char c) { return !std::isspace(c); };
  auto last = buf.rend();
  auto i = std::find_if(buf.rbegin(), last, not_ws);
  return (i != last) ? *i : '\0';
}

class impl : public byte_writer, public internal::fast_pimpl<impl> {
public:
  // -- member types -----------------------------------------------------------

  using super = byte_writer;

  // -- constructors, destructors, and assignment operators --------------------

  impl(actor_system* sys, serializer* parent) : sys_(sys), parent_(parent) {
    // Reserve some reasonable storage for the character buffer. JSON grows
    // quickly, so we can start at 1kb to avoid a couple of small allocations in
    // the beginning.
    buf_.reserve(1024);
    // Even heavily nested objects should fit into 32 levels of nesting.
    stack_.reserve(32);
    // Placeholder for what is to come.
    push();
  }

  // -- properties -------------------------------------------------------------

  const_byte_span bytes() const override {
    return {reinterpret_cast<const std::byte*>(buf_.data()), buf_.size()};
  }

  [[nodiscard]] std::string_view str() const noexcept {
    return {buf_.data(), buf_.size()};
  }

  [[nodiscard]] size_t indentation() const noexcept {
    return indentation_factor_;
  }

  void indentation(size_t factor) noexcept {
    indentation_factor_ = factor;
  }

  [[nodiscard]] bool compact() const noexcept {
    return indentation_factor_ == 0;
  }

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

  [[nodiscard]] const type_id_mapper* mapper() const noexcept {
    return mapper_;
  }

  void mapper(const type_id_mapper* ptr) noexcept {
    mapper_ = ptr;
  }

  // -- modifiers --------------------------------------------------------------

  void reset() override {
    buf_.clear();
    stack_.clear();
    push();
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
      add(R"_("@type": )_");
      pop();
      CAF_ASSERT(top() == internal::json_node::element);
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

  bool end_object() override {
    return end_associative_array();
  }

  bool begin_field(std::string_view name) override {
    if (begin_key_value_pair()) {
      CAF_ASSERT(top() == internal::json_node::key);
      add('"');
      add(name);
      add("\": ");
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
          push(internal::json_node::member);
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
      CAF_ASSERT(top() == internal::json_node::key);
      add('"');
      add(name);
      add("\": ");
      pop();
      CAF_ASSERT(top() == internal::json_node::element);
      if (!is_present) {
        add("null");
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
      CAF_ASSERT(top() == internal::json_node::key);
      add("\"@");
      add(name);
      add(field_type_suffix_);
      add("\": ");
      pop();
      CAF_ASSERT(top() == internal::json_node::element);
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
    sep();
    auto t = top();
    switch (t) {
      case internal::json_node::object:
        push(internal::json_node::member);
        push(internal::json_node::element);
        push(internal::json_node::key);
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

  bool end_key_value_pair() override {
    return pop_if(internal::json_node::member);
  }

  bool begin_sequence(size_t) override {
    switch (top()) {
      default:
        err_ = make_error(sec::runtime_error, "unexpected begin_sequence");
        return false;
      case internal::json_node::element:
        unsafe_morph(internal::json_node::array);
        break;
      case internal::json_node::array:
        push(internal::json_node::array);
        break;
    }
    add('[');
    ++indentation_level_;
    nl();
    return true;
  }

  bool end_sequence() override {
    if (pop_if(internal::json_node::array)) {
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

  bool begin_associative_array(size_t) override {
    switch (top()) {
      default:
        err_ = format_to_error(sec::runtime_error,
                               "{}::{}: unexpected begin_object "
                               "or begin_associative_array",
                               class_name, __func__);
        return false;
      case internal::json_node::element:
        unsafe_morph(internal::json_node::object);
        break;
      case internal::json_node::array:
        sep();
        push(internal::json_node::object);
        break;
    }
    add('{');
    ++indentation_level_;
    nl();
    return true;
  }

  bool end_associative_array() override {
    if (pop_if(internal::json_node::object)) {
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

  bool value(std::byte x) override {
    return number(std::to_integer<uint8_t>(x));
  }

  bool value(bool x) override {
    auto add_str = [this, x] {
      if (x)
        add("true");
      else
        add("false");
    };
    switch (top()) {
      case internal::json_node::element:
        add_str();
        pop();
        return true;
      case internal::json_node::key:
        add('"');
        add_str();
        add("\": ");
        return true;
      case internal::json_node::array:
        sep();
        add_str();
        return true;
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
        detail::print_escaped(buf_, x);
        pop();
        return true;
      case internal::json_node::key:
        detail::print_escaped(buf_, x);
        add(": ");
        pop();
        return true;
      case internal::json_node::array:
        sep();
        detail::print_escaped(buf_, x);
        return true;
      default:
        fail(internal::json_node::string);
        return false;
    }
  }

  bool value(const std::u16string&) override {
    err_ = make_error(sec::unsupported_operation,
                      "u16string not supported yet by caf::json_writer");
    return false;
  }

  bool value(const std::u32string&) override {
    err_ = make_error(sec::unsupported_operation,
                      "u32string not supported yet by caf::json_writer");
    return false;
  }

  bool value(const_byte_span x) override {
    switch (top()) {
      case internal::json_node::element:
        add('"');
        detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                           x.size());
        add('"');
        pop();
        return true;
      case internal::json_node::key:
        unsafe_morph(internal::json_node::element);
        add('"');
        detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                           x.size());
        add("\": ");
        pop();
        return true;
      case internal::json_node::array:
        sep();
        add('"');
        detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                           x.size());
        add('"');
        return true;
      default:
        fail(internal::json_node::string);
        return false;
    }
  }

  bool value(const strong_actor_ptr& ptr) override {
    // These are customization points for the deserializer. Client code may
    // inherit from json_writer and override these member functions. Hence, we
    // need to dispatch to the parent class.
    return parent_->value(ptr);
  }

  bool value(const weak_actor_ptr& ptr) override {
    // Same as above.
    return parent_->value(ptr);
  }

private:
  // -- implementation details -------------------------------------------------

  template <class T>
  bool number(T x) {
    switch (top()) {
      case internal::json_node::element:
        detail::print(buf_, x);
        pop();
        return true;
      case internal::json_node::key:
        add('"');
        detail::print(buf_, x);
        add("\": ");
        return true;
      case internal::json_node::array:
        sep();
        detail::print(buf_, x);
        return true;
      default:
        fail(internal::json_node::number);
        return false;
    }
  }

  // -- state management -------------------------------------------------------

  // Returns the current top of the stack or `null` if empty.
  internal::json_node top() {
    if (!stack_.empty())
      return stack_.back().t;
    else
      return internal::json_node::null;
  }

  // Enters a new level of nesting.
  void push(internal::json_node t = internal::json_node::element) {
    auto tmp = entry{t, false};
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
  bool pop_if(internal::json_node t) {
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

  // Backs up one level of nesting but checks that the top is `t` afterwards.
  bool pop_if_next(internal::json_node t) {
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
  bool morph(internal::json_node t) {
    internal::json_node unused;
    return morph(t, unused);
  }

  // Tries to morph the current top of the stack to t. Stores the previous value
  // to `prev`.
  bool morph(internal::json_node t, internal::json_node& prev) {
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

  // Morphs the current top of the stack to t without performing *any* checks.
  void unsafe_morph(internal::json_node t) {
    stack_.back().t = t;
  }

  // Sets an error reason that the inspector failed to write a t.
  void fail(internal::json_node t) {
    err_ = format_to_error(sec::runtime_error,
                           "failed to write a {}: "
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

  // -- printing ---------------------------------------------------------------

  // Adds a newline unless `compact() == true`.
  void nl() {
    if (indentation_factor_ > 0) {
      buf_.push_back('\n');
      buf_.insert(buf_.end(), indentation_factor_ * indentation_level_, ' ');
    }
  }

  // Adds `c` to the output buffer.
  void add(char c) {
    buf_.push_back(c);
  }

  // Adds `str` to the output buffer.
  void add(std::string_view str) {
    buf_.insert(buf_.end(), str.begin(), str.end());
  }

  // Adds a separator to the output buffer unless the current entry is empty.
  // The separator is just a comma when in compact mode and otherwise a comma
  // followed by a newline.
  void sep() {
    CAF_ASSERT(top() == internal::json_node::element
               || top() == internal::json_node::object
               || top() == internal::json_node::array);
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

  // -- member variables -------------------------------------------------------

  // The actor system this writer belongs to.
  actor_system* sys_ = nullptr;

  // The current level of indentation.
  size_t indentation_level_ = 0;

  // The number of whitespaces to add per indentation level.
  size_t indentation_factor_ = 0;

  // Buffer for producing the JSON output.
  std::vector<char> buf_;

  struct entry {
    internal::json_node t;
    bool filled;
    friend bool operator==(entry x, internal::json_node y) noexcept {
      return x.t == y;
    };
  };

  // Bookkeeping for where we are in the current object.
  std::vector<entry> stack_;

  // Configures whether we omit empty fields entirely (true) or render empty
  // fields as `$field: null` (false).
  bool skip_empty_fields_ = skip_empty_fields_default;

  // Configures whether we omit the top-level `@type` annotation.
  bool skip_object_type_annotation_ = skip_object_type_annotation_default;

  // Configures how we generate type annotations for fields.
  std::string_view field_type_suffix_ = field_type_suffix_default;

  // The mapper implementation we use by default.
  default_type_id_mapper default_mapper_;

  // Configures which ID mapper we use to translate between type IDs and names.
  const type_id_mapper* mapper_ = &default_mapper_;

  /// The last error that occurred.
  error err_;

  serializer* parent_;
};

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

json_writer::json_writer() {
  impl::construct(impl_, nullptr, this);
}

json_writer::json_writer(actor_system& sys) {
  impl::construct(impl_, &sys, this);
}

json_writer::~json_writer() {
  impl::destruct(impl_);
}

// -- properties ---------------------------------------------------------------

const_byte_span json_writer::bytes() const {
  return impl::cast(impl_).bytes();
}

std::string_view json_writer::str() const noexcept {
  return impl::cast(impl_).str();
}

size_t json_writer::indentation() const noexcept {
  return impl::cast(impl_).indentation();
}

void json_writer::indentation(size_t factor) noexcept {
  impl::cast(impl_).indentation(factor);
}

bool json_writer::compact() const noexcept {
  return impl::cast(impl_).compact();
}

bool json_writer::skip_empty_fields() const noexcept {
  return impl::cast(impl_).skip_empty_fields();
}

void json_writer::skip_empty_fields(bool value) noexcept {
  impl::cast(impl_).skip_empty_fields(value);
}

bool json_writer::skip_object_type_annotation() const noexcept {
  return impl::cast(impl_).skip_object_type_annotation();
}

void json_writer::skip_object_type_annotation(bool value) noexcept {
  impl::cast(impl_).skip_object_type_annotation(value);
}

std::string_view json_writer::field_type_suffix() const noexcept {
  return impl::cast(impl_).field_type_suffix();
}

void json_writer::field_type_suffix(std::string_view suffix) noexcept {
  impl::cast(impl_).field_type_suffix(suffix);
}

const type_id_mapper* json_writer::mapper() const noexcept {
  return impl::cast(impl_).mapper();
}

void json_writer::mapper(const type_id_mapper* ptr) noexcept {
  impl::cast(impl_).mapper(ptr);
}

// -- modifiers ----------------------------------------------------------------

void json_writer::reset() {
  impl::cast(impl_).reset();
}

// -- overrides ----------------------------------------------------------------

void json_writer::set_error(error stop_reason) {
  impl::cast(impl_).set_error(std::move(stop_reason));
}

error& json_writer::get_error() noexcept {
  return impl::cast(impl_).get_error();
}

caf::actor_system* json_writer::sys() const noexcept {
  return impl::cast(impl_).sys();
}

bool json_writer::has_human_readable_format() const noexcept {
  return impl::cast(impl_).has_human_readable_format();
}

bool json_writer::begin_object(type_id_t id, std::string_view name) {
  return impl::cast(impl_).begin_object(id, name);
}

bool json_writer::end_object() {
  return impl::cast(impl_).end_object();
}

bool json_writer::begin_field(std::string_view name) {
  return impl::cast(impl_).begin_field(name);
}

bool json_writer::begin_field(std::string_view name, bool is_present) {
  return impl::cast(impl_).begin_field(name, is_present);
}

bool json_writer::begin_field(std::string_view name,
                              span<const type_id_t> types, size_t index) {
  return impl::cast(impl_).begin_field(name, types, index);
}

bool json_writer::begin_field(std::string_view name, bool is_present,
                              span<const type_id_t> types, size_t index) {
  return impl::cast(impl_).begin_field(name, is_present, types, index);
}

bool json_writer::end_field() {
  return impl::cast(impl_).end_field();
}

bool json_writer::begin_tuple(size_t size) {
  return impl::cast(impl_).begin_tuple(size);
}

bool json_writer::end_tuple() {
  return impl::cast(impl_).end_tuple();
}

bool json_writer::begin_key_value_pair() {
  return impl::cast(impl_).begin_key_value_pair();
}

bool json_writer::end_key_value_pair() {
  return impl::cast(impl_).end_key_value_pair();
}

bool json_writer::begin_sequence(size_t size) {
  return impl::cast(impl_).begin_sequence(size);
}

bool json_writer::end_sequence() {
  return impl::cast(impl_).end_sequence();
}

bool json_writer::begin_associative_array(size_t size) {
  return impl::cast(impl_).begin_associative_array(size);
}

bool json_writer::end_associative_array() {
  return impl::cast(impl_).end_associative_array();
}

bool json_writer::value(std::byte x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(bool x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(int8_t x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(uint8_t x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(int16_t x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(uint16_t x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(int32_t x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(uint32_t x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(int64_t x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(uint64_t x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(float x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(double x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(long double x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(std::string_view x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(const std::u16string& x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(const std::u32string& x) {
  return impl::cast(impl_).value(x);
}

bool json_writer::value(const_byte_span x) {
  return impl::cast(impl_).value(x);
}

} // namespace caf
