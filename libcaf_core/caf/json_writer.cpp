// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_writer.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_handle_codec.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/default_actor_handle_codec.hpp"
#include "caf/detail/print.hpp"
#include "caf/format_to_error.hpp"
#include "caf/internal/json_node.hpp"
#include "caf/serializer.hpp"
#include "caf/type_id_list.hpp"

#include <algorithm>
#include <utility>

namespace {

static constexpr const char class_name[] = "caf::json_writer_base";

char last_non_ws_char(const std::vector<char>& buf) {
  auto not_ws = [](char c) {
    return !std::isspace(static_cast<unsigned char>(c));
  };
  auto last = buf.rend();
  auto i = std::find_if(buf.rbegin(), last, not_ws);
  return (i != last) ? *i : '\0';
}

} // namespace

namespace caf::internal {

class json_writer_stack_entry {
public:
  internal::json_node t;
  bool filled;

  friend bool operator==(json_writer_stack_entry x,
                         internal::json_node y) noexcept {
    return x.t == y;
  }
};

class json_writer_util {
public:
  static internal::json_node top(json_writer_base& writer) {
    if (!writer.stack_.empty())
      return writer.stack_.back().t;
    return internal::json_node::null;
  }

  static void push(json_writer_base& writer, internal::json_node t) {
    writer.stack_.push_back(json_writer_stack_entry{t, false});
  }

  static bool pop(json_writer_base& writer) {
    if (!writer.stack_.empty()) {
      writer.stack_.pop_back();
      return true;
    }
    writer.err_ = make_error(sec::runtime_error,
                             "pop() called with an empty stack: begin/end "
                             "mismatch");
    return false;
  }

  static bool pop_if(json_writer_base& writer, internal::json_node t) {
    auto& stack = writer.stack_;
    if (!stack.empty() && stack.back() == t) {
      stack.pop_back();
      return true;
    }
    if (stack.empty()) {
      writer.err_ = format_to_error(sec::runtime_error,
                                    "pop_if failed: expected {} "
                                    "but found an empty stack",
                                    as_json_type_name(t));
    } else {
      writer.err_ = format_to_error(sec::runtime_error,
                                    "pop_if failed: expected {} but found {}",
                                    as_json_type_name(t),
                                    as_json_type_name(stack.back().t));
    }
    return false;
  }

  static bool morph(json_writer_base& writer, internal::json_node t,
                    internal::json_node& prev) {
    auto& stack = writer.stack_;
    if (!stack.empty()) {
      if (can_morph(stack.back().t, t)) {
        prev = stack.back().t;
        stack.back().t = t;
        return true;
      }
      std::string err_msg = "cannot convert ";
      err_msg += as_json_type_name(stack.back().t);
      err_msg += " to ";
      err_msg += as_json_type_name(t);
      writer.err_ = make_error(sec::runtime_error, std::move(err_msg));
      return false;
    }
    writer.err_ = make_error(sec::runtime_error,
                             "mismatched begin/end calls on the JSON "
                             "inspector");
    return false;
  }

  [[maybe_unused]] static bool morph(json_writer_base& writer,
                                     internal::json_node t) {
    internal::json_node unused;
    return morph(writer, t, unused);
  }

  static void unsafe_morph(json_writer_base& writer, internal::json_node t) {
    writer.stack_.back().t = t;
  }

  static void fail(json_writer_base& writer, internal::json_node t) {
    writer.err_ = format_to_error(sec::runtime_error,
                                  "failed to write a {}: "
                                  "invalid position (begin/end mismatch?)",
                                  as_json_type_name(t));
  }

  static bool inside_object(const json_writer_base& writer) noexcept {
    auto is_object = [](const json_writer_stack_entry& x) {
      return x.t == internal::json_node::object;
    };
    return std::ranges::any_of(writer.stack_, is_object);
  }

  static void nl(json_writer_base& writer) {
    if (writer.indentation_factor_ > 0) {
      writer.buf_.push_back('\n');
      writer.buf_.insert(writer.buf_.end(),
                         writer.indentation_factor_ * writer.indentation_level_,
                         ' ');
    }
  }

  static void add(json_writer_base& writer, char c) {
    writer.buf_.push_back(c);
  }

  static void add(json_writer_base& writer, std::string_view str) {
    writer.buf_.insert(writer.buf_.end(), str.begin(), str.end());
  }

  static void sep(json_writer_base& writer) {
    CAF_ASSERT(top(writer) == internal::json_node::element
               || top(writer) == internal::json_node::object
               || top(writer) == internal::json_node::array);
    if (writer.stack_.back().filled) {
      if (writer.indentation_factor_ > 0) {
        add(writer, ",\n");
        writer.buf_.insert(
          writer.buf_.end(),
          writer.indentation_factor_ * writer.indentation_level_, ' ');
      } else {
        add(writer, ", ");
      }
    } else {
      writer.stack_.back().filled = true;
    }
  }

  template <class T>
  static bool number(json_writer_base& writer, T x) {
    switch (top(writer)) {
      case internal::json_node::element:
        detail::print(writer.buf_, x);
        pop(writer);
        return true;
      case internal::json_node::key:
        add(writer, '"');
        detail::print(writer.buf_, x);
        add(writer, "\": ");
        return true;
      case internal::json_node::array:
        sep(writer);
        detail::print(writer.buf_, x);
        return true;
      default:
        fail(writer, internal::json_node::number);
        return false;
    }
  }
};

} // namespace caf::internal

namespace caf {

json_writer_base::json_writer_base()
  : json_writer_base(get_default_type_id_mapper()) {
  // nop
}

json_writer_base::json_writer_base(const type_id_mapper& mapper)
  : mapper_(&mapper) {
  /// All flags are initially set to false.
  std::memset(&flags_, 0, sizeof(flags_));
  // Reserve some reasonable storage for the character buffer. JSON grows
  // quickly, so we can start at 1kb to avoid a couple of small allocations in
  // the beginning.
  buf_.reserve(1024);
  // Even heavily nested objects should fit into 32 levels of nesting.
  stack_.reserve(32);
  // Placeholder for what is to come.
  util::push(*this, internal::json_node::element);
}

json_writer_base::~json_writer_base() noexcept {
  // nop
}

std::string_view json_writer_base::str() const noexcept {
  return {buf_.data(), buf_.size()};
}

void json_writer_base::mapper(const type_id_mapper* ptr) noexcept {
  mapper_ = ptr != nullptr ? ptr : &get_default_type_id_mapper();
}

[[nodiscard]] size_t json_writer_base::indentation() const noexcept {
  return indentation_factor_;
}

void json_writer_base::indentation(size_t factor) noexcept {
  indentation_factor_ = factor;
}

[[nodiscard]] bool json_writer_base::compact() const noexcept {
  return indentation_factor_ == 0;
}

[[nodiscard]] bool json_writer_base::skip_empty_fields() const noexcept {
  return !flags_.keep_empty_fields;
}

void json_writer_base::skip_empty_fields(bool value) noexcept {
  flags_.keep_empty_fields = !value;
}

bool json_writer_base::skip_object_type_annotation() const noexcept {
  return flags_.skip_object_type_annotation;
}

void json_writer_base::skip_object_type_annotation(bool value) noexcept {
  flags_.skip_object_type_annotation = value;
}

std::string_view json_writer_base::field_type_suffix() const noexcept {
  return field_type_suffix_;
}

void json_writer_base::field_type_suffix(std::string_view suffix) noexcept {
  field_type_suffix_ = suffix;
}

void json_writer_base::reset() {
  buf_.clear();
  stack_.clear();
  util::push(*this, internal::json_node::element);
}

void json_writer_base::set_error(error stop_reason) {
  err_ = std::move(stop_reason);
}

error& json_writer_base::get_error() noexcept {
  return err_;
}

bool json_writer_base::has_human_readable_format() const noexcept {
  return true;
}

std::string_view json_writer_base::to_type_name(type_id_t id) const {
  return (*mapper_)(id);
}

bool json_writer_base::begin_object(type_id_t id, std::string_view name) {
  auto add_type_annotation = [this, id, name] {
    CAF_ASSERT(util::top(*this) == internal::json_node::key);
    util::add(*this, R"_("@type": )_");
    util::pop(*this);
    CAF_ASSERT(util::top(*this) == internal::json_node::element);
    auto tname = to_type_name(id);
    util::add(*this, '"');
    util::add(*this, tname.empty() ? name : tname);
    util::add(*this, '"');
    util::pop(*this);
    return true;
  };
  if (flags_.skip_object_type_annotation || util::inside_object(*this)) {
    return begin_associative_array(0);
  }
  return begin_associative_array(0) // Put opening paren, ...
         && begin_key_value_pair()  // ... add implicit @type member, ..
         && add_type_annotation()   // ... write content ...
         && end_key_value_pair();   // ... and wait for next field.
}

bool json_writer_base::end_object() {
  return end_associative_array();
}

bool json_writer_base::begin_field(std::string_view name) {
  if (begin_key_value_pair()) {
    CAF_ASSERT(util::top(*this) == internal::json_node::key);
    util::add(*this, '"');
    util::add(*this, name);
    util::add(*this, "\": ");
    util::pop(*this);
    CAF_ASSERT(util::top(*this) == internal::json_node::element);
    return true;
  }
  return false;
}

bool json_writer_base::begin_field(std::string_view name, bool is_present) {
  if (!flags_.keep_empty_fields && !is_present) {
    auto t = util::top(*this);
    switch (t) {
      case internal::json_node::object:
        util::push(*this, internal::json_node::member);
        return true;
      default: {
        std::string err_msg = "expected object, found ";
        err_msg += as_json_type_name(t);
        err_ = format_to_error(sec::runtime_error, "{}::{}: {}", class_name,
                               __func__, err_msg);
        return false;
      }
    }
  } else if (begin_key_value_pair()) {
    CAF_ASSERT(util::top(*this) == internal::json_node::key);
    util::add(*this, '"');
    util::add(*this, name);
    util::add(*this, "\": ");
    util::pop(*this);
    CAF_ASSERT(util::top(*this) == internal::json_node::element);
    if (!is_present) {
      util::add(*this, "null");
      util::pop(*this);
    }
    return true;
  }
  return false;
}

bool json_writer_base::begin_field(std::string_view name,
                                   std::span<const type_id_t> types,
                                   size_t index) {
  if (index >= types.size()) {
    err_ = make_error(sec::runtime_error, "index >= types.size()");
    return false;
  }
  if (begin_key_value_pair()) {
    CAF_ASSERT(util::top(*this) == internal::json_node::key);
    util::add(*this, "\"@");
    util::add(*this, name);
    util::add(*this, field_type_suffix_);
    util::add(*this, "\": ");
    util::pop(*this);
    CAF_ASSERT(util::top(*this) == internal::json_node::element);
    util::pop(*this);
    auto tname = to_type_name(types[index]);
    if (tname.empty()) {
      err_ = make_error(sec::runtime_error, "failed to retrieve type name");
      return false;
    }
    util::add(*this, '"');
    util::add(*this, tname);
    util::add(*this, '"');
    return end_key_value_pair() && begin_field(name);
  }
  return false;
}

bool json_writer_base::begin_field(std::string_view name, bool is_present,
                                   std::span<const type_id_t> types,
                                   size_t index) {
  if (is_present) {
    return begin_field(name, types, index);
  }
  return begin_field(name, is_present);
}

bool json_writer_base::end_field() {
  return end_key_value_pair();
}

bool json_writer_base::begin_tuple(size_t size) {
  return begin_sequence(size);
}

bool json_writer_base::end_tuple() {
  return end_sequence();
}

bool json_writer_base::begin_key_value_pair() {
  util::sep(*this);
  auto t = util::top(*this);
  switch (t) {
    case internal::json_node::object:
      util::push(*this, internal::json_node::member);
      util::push(*this, internal::json_node::element);
      util::push(*this, internal::json_node::key);
      return true;
    default: {
      std::string err_msg = "expected object, found ";
      err_msg += as_json_type_name(t);
      err_ = format_to_error(sec::runtime_error, "{}::{}: {}", class_name,
                             __func__, std::move(err_msg));
      return false;
    }
  }
}

bool json_writer_base::end_key_value_pair() {
  return util::pop_if(*this, internal::json_node::member);
}

bool json_writer_base::begin_sequence(size_t) {
  switch (util::top(*this)) {
    default:
      err_ = make_error(sec::runtime_error, "unexpected begin_sequence");
      return false;
    case internal::json_node::element:
      util::unsafe_morph(*this, internal::json_node::array);
      break;
    case internal::json_node::array:
      util::sep(*this);
      util::push(*this, internal::json_node::array);
      break;
  }
  util::add(*this, '[');
  ++indentation_level_;
  util::nl(*this);
  return true;
}

bool json_writer_base::end_sequence() {
  if (util::pop_if(*this, internal::json_node::array)) {
    --indentation_level_;
    // Check whether the array was empty and compress the output in that case.
    if (last_non_ws_char(buf_) == '[') {
      while (std::isspace(buf_.back()))
        buf_.pop_back();
    } else {
      util::nl(*this);
    }
    util::add(*this, ']');
    return true;
  }
  return false;
}

bool json_writer_base::begin_associative_array(size_t) {
  switch (util::top(*this)) {
    default:
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: unexpected begin_object "
                             "or begin_associative_array",
                             class_name, __func__);
      return false;
    case internal::json_node::element:
      util::unsafe_morph(*this, internal::json_node::object);
      break;
    case internal::json_node::array:
      util::sep(*this);
      util::push(*this, internal::json_node::object);
      break;
  }
  util::add(*this, '{');
  ++indentation_level_;
  util::nl(*this);
  return true;
}

bool json_writer_base::end_associative_array() {
  if (util::pop_if(*this, internal::json_node::object)) {
    --indentation_level_;
    // Check whether the array was empty and compress the output in that case.
    if (last_non_ws_char(buf_) == '{') {
      while (std::isspace(buf_.back()))
        buf_.pop_back();
    } else {
      util::nl(*this);
    }
    util::add(*this, '}');
    if (!stack_.empty())
      stack_.back().filled = true;
    return true;
  }
  return false;
}

bool json_writer_base::value(std::byte x) {
  return util::number(*this, std::to_integer<uint8_t>(x));
}

bool json_writer_base::value(bool x) {
  auto add_str = [this, x] {
    if (x)
      util::add(*this, "true");
    else
      util::add(*this, "false");
  };
  switch (util::top(*this)) {
    case internal::json_node::element:
      add_str();
      util::pop(*this);
      return true;
    case internal::json_node::key:
      util::add(*this, '"');
      add_str();
      util::add(*this, "\": ");
      return true;
    case internal::json_node::array:
      util::sep(*this);
      add_str();
      return true;
    default:
      util::fail(*this, internal::json_node::boolean);
      return false;
  }
}

bool json_writer_base::value(int8_t x) {
  return util::number(*this, x);
}

bool json_writer_base::value(uint8_t x) {
  return util::number(*this, x);
}

bool json_writer_base::value(int16_t x) {
  return util::number(*this, x);
}

bool json_writer_base::value(uint16_t x) {
  return util::number(*this, x);
}

bool json_writer_base::value(int32_t x) {
  return util::number(*this, x);
}

bool json_writer_base::value(uint32_t x) {
  return util::number(*this, x);
}

bool json_writer_base::value(int64_t x) {
  return util::number(*this, x);
}

bool json_writer_base::value(uint64_t x) {
  return util::number(*this, x);
}

bool json_writer_base::value(float x) {
  return util::number(*this, x);
}

bool json_writer_base::value(double x) {
  return util::number(*this, x);
}

bool json_writer_base::value(long double x) {
  return util::number(*this, x);
}

bool json_writer_base::value(std::string_view x) {
  switch (util::top(*this)) {
    case internal::json_node::element:
      detail::print_escaped(buf_, x);
      util::pop(*this);
      return true;
    case internal::json_node::key:
      detail::print_escaped(buf_, x);
      util::add(*this, ": ");
      util::pop(*this);
      return true;
    case internal::json_node::array:
      util::sep(*this);
      detail::print_escaped(buf_, x);
      return true;
    default:
      util::fail(*this, internal::json_node::string);
      return false;
  }
}

bool json_writer_base::value(const std::u16string&) {
  err_ = make_error(sec::unsupported_operation,
                    "u16string not supported yet by caf::json_writer");
  return false;
}

bool json_writer_base::value(const std::u32string&) {
  err_ = make_error(sec::unsupported_operation,
                    "u32string not supported yet by caf::json_writer");
  return false;
}

bool json_writer_base::value(const_byte_span x) {
  switch (util::top(*this)) {
    case internal::json_node::element:
      util::add(*this, '"');
      detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                         x.size());
      util::add(*this, '"');
      util::pop(*this);
      return true;
    case internal::json_node::key:
      util::unsafe_morph(*this, internal::json_node::element);
      util::add(*this, '"');
      detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                         x.size());
      util::add(*this, "\": ");
      util::pop(*this);
      return true;
    case internal::json_node::array:
      util::sep(*this);
      util::add(*this, '"');
      detail::append_hex(buf_, reinterpret_cast<const void*>(x.data()),
                         x.size());
      util::add(*this, '"');
      return true;
    default:
      util::fail(*this, internal::json_node::string);
      return false;
  }
}

bool json_writer_base::value(type_id_list xs) {
  return serializer::value(xs);
}

bool json_writer::value(const strong_actor_ptr& ptr) {
  if (context_ != nullptr) {
    detail::default_actor_handle_codec codec{*context_};
    return codec.save(*this, ptr);
  }
  emplace_error(sec::no_context);
  return false;
}

bool json_writer_v2::value(const strong_actor_ptr& ptr) {
  if (codec_ != nullptr) {
    return codec_->save(*this, ptr);
  }
  emplace_error(sec::no_actor_handle_codec);
  return false;
}

} // namespace caf
