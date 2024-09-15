// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_reader.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/print.hpp"
#include "caf/format_to_error.hpp"
#include "caf/string_algorithms.hpp"

#include <fstream>

namespace {

static constexpr const char class_name[] = "caf::json_reader";

std::string_view pretty_name(caf::json_reader::position pos) {
  switch (pos) {
    default:
      return "invalid input";
    case caf::json_reader::position::value:
      return "json::value";
    case caf::json_reader::position::object:
      return "json::object";
    case caf::json_reader::position::null:
      return "null";
    case caf::json_reader::position::key:
      return "json::key";
    case caf::json_reader::position::sequence:
      return "json::array";
    case caf::json_reader::position::members:
      return "json::members";
  }
}

std::string_view type_name_from(const caf::detail::json::value& got) {
  using namespace std::literals;
  using caf::detail::json::value;
  switch (got.data.index()) {
    case value::integer_index:
    case value::unsigned_index:
      return "json::integer"sv;
    case value::double_index:
      return "json::real"sv;
    case value::bool_index:
      return "json::boolean"sv;
    case value::string_index:
      return "json::string"sv;
    case value::array_index:
      return "json::array"sv;
    case value::object_index:
      return "json::object"sv;
    default:
      return "json::null"sv;
  }
}

const caf::detail::json::member*
find_member(const caf::detail::json::object* obj, std::string_view key) {
  for (const auto& member : *obj)
    if (member.key == key)
      return &member;
  return nullptr;
}

std::string_view field_type(const caf::detail::json::object* obj,
                            std::string_view name, std::string_view suffix) {
  namespace json = caf::detail::json;
  for (const auto& member : *obj) {
    if (member.val && member.val->data.index() == json::value::string_index
        && !member.key.empty() && member.key[0] == '@') {
      auto key = member.key;
      key.remove_prefix(1);
      if (caf::starts_with(key, name)) {
        key.remove_prefix(name.size());
        if (key == suffix)
          return std::get<std::string_view>(member.val->data);
      }
    }
  }
  return {};
}

} // namespace

#define FN_DECL static constexpr const char* fn = __func__

#define INVALID_AND_PAST_THE_END_CASES                                         \
  case position::invalid:                                                      \
    err_ = format_to_error(sec::runtime_error,                                 \
                           "{}::{}: found an invalid position in field {}",    \
                           class_name, fn, current_field_name());              \
    return false;                                                              \
  case position::past_the_end:                                                 \
    err_ = format_to_error(sec::runtime_error,                                 \
                           "{}::{}: unexpected end of input in field {}",      \
                           class_name, fn, current_field_name());              \
    return false;

#define SCOPE(expected_position)                                               \
  if (auto got = pos(); got != expected_position) {                            \
    err_ = format_to_error(sec::runtime_error,                                 \
                           "{}::{}: expected type {}, got {} in field {}",     \
                           class_name, __func__,                               \
                           pretty_name(expected_position), pretty_name(got),   \
                           current_field_name());                              \
    return false;                                                              \
  }

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

json_reader::json_reader() {
  field_.reserve(8);
  has_human_readable_format_ = true;
}

json_reader::json_reader(actor_system& sys) : super(sys) {
  field_.reserve(8);
  has_human_readable_format_ = true;
}

json_reader::~json_reader() {
  // nop
}

// -- modifiers --------------------------------------------------------------

bool json_reader::load(std::string_view json_text) {
  reset();
  string_parser_state ps{json_text.begin(), json_text.end()};
  root_ = detail::json::parse_shallow(ps, &buf_);
  if (ps.code != pec::success) {
    err_ = ps.error();
    st_ = nullptr;
    return false;
  }
  err_.reset();
  detail::monotonic_buffer_resource::allocator<stack_type> alloc{&buf_};
  st_ = new (alloc.allocate(1)) stack_type(stack_allocator{&buf_});
  st_->reserve(16);
  st_->emplace_back(root_);
  return true;
}

bool json_reader::load_file(const char* path) {
  using iterator_t = std::istreambuf_iterator<char>;
  reset();
  std::ifstream input{path};
  if (!input.is_open()) {
    err_ = sec::cannot_open_file;
    return false;
  }
  detail::json::file_parser_state ps{iterator_t{input}, iterator_t{}};
  root_ = detail::json::parse(ps, &buf_);
  if (ps.code != pec::success) {
    err_ = ps.error();
    st_ = nullptr;
    return false;
  }
  err_.reset();
  detail::monotonic_buffer_resource::allocator<stack_type> alloc{&buf_};
  st_ = new (alloc.allocate(1)) stack_type(stack_allocator{&buf_});
  st_->reserve(16);
  st_->emplace_back(root_);
  return true;
}

void json_reader::revert() {
  if (st_) {
    CAF_ASSERT(root_ != nullptr);
    err_.reset();
    st_->clear();
    st_->emplace_back(root_);
    field_.clear();
  }
}

void json_reader::reset() {
  buf_.reclaim();
  st_ = nullptr;
  err_.reset();
  field_.clear();
}

// -- interface functions ------------------------------------------------------

bool json_reader::fetch_next_object_type(type_id_t& type) {
  std::string_view type_name;
  if (fetch_next_object_name(type_name)) {
    if (auto id = (*mapper_)(type_name); id != invalid_type_id) {
      type = id;
      return true;
    } else {
      std::string what = "no type ID available for @type: ";
      what.insert(what.end(), type_name.begin(), type_name.end());
      err_ = format_to_error(
        sec::runtime_error,
        "{}::{}: no type ID available for @type: {} in field {}", class_name,
        __func__, type_name, current_field_name());
      return false;
    }
  } else {
    return false;
  }
}

bool json_reader::fetch_next_object_name(std::string_view& type_name) {
  FN_DECL;
  return consume<false>(fn, [this, &type_name](const detail::json::value& val) {
    if (val.data.index() == detail::json::value::object_index) {
      auto& obj = std::get<detail::json::object>(val.data);
      if (auto mem_ptr = find_member(&obj, "@type")) {
        if (mem_ptr->val->data.index() == detail::json::value::string_index) {
          type_name = std::get<std::string_view>(mem_ptr->val->data);
          return true;
        }
        err_ = format_to_error(
          sec::runtime_error,
          "{}::{}: expected a string argument to @type in field {}", class_name,
          fn, current_field_name());
        return false;
      }
      err_ = format_to_error(sec::runtime_error, class_name, fn,
                             current_field_name(), "found no @type member");
      return false;
    }
    err_ = format_to_error(
      sec::runtime_error,
      "{}::{}: expected type json::object, got {} in field {}", class_name, fn,
      type_name_from(val), current_field_name());
    return false;
  });
}

bool json_reader::begin_object(type_id_t, std::string_view) {
  FN_DECL;
  return consume<false>(fn, [this](const detail::json::value& val) {
    if (val.data.index() == detail::json::value::object_index) {
      push(&std::get<detail::json::object>(val.data));
      return true;
    } else {
      err_ = format_to_error(
        sec::runtime_error,
        "{}::{}: expected type json::object, got {} in field {}", class_name,
        fn, type_name_from(val), current_field_name());
      return false;
    }
  });
}

bool json_reader::end_object() {
  FN_DECL;
  SCOPE(position::object);
  pop();
  auto current_pos = pos();
  switch (current_pos) {
    INVALID_AND_PAST_THE_END_CASES
    case position::value:
      pop();
      return true;
    case position::sequence:
      top<position::sequence>().advance();
      return true;
    default:
      err_ = format_to_error(
        sec::runtime_error,
        "{}::{}: expected type json::value or json::array, got {} in field {}",
        class_name, fn, pretty_name(current_pos), current_field_name());
      return false;
  }
}

bool json_reader::begin_field(std::string_view name) {
  SCOPE(position::object);
  field_.push_back(name);
  if (auto member = find_member(top<position::object>(), name)) {
    push(member->val);
    return true;
  } else {
    err_ = format_to_error(sec::runtime_error,
                           "{}::{}: mandatory key {} missing in field {}",
                           class_name, __func__, name, current_field_name());
    return false;
  }
}

bool json_reader::begin_field(std::string_view name, bool& is_present) {
  SCOPE(position::object);
  field_.push_back(name);
  if (auto member = find_member(top<position::object>(), name);
      member != nullptr
      && member->val->data.index() != detail::json::value::null_index) {
    push(member->val);
    is_present = true;
  } else {
    is_present = false;
  }
  return true;
}

bool json_reader::begin_field(std::string_view name,
                              span<const type_id_t> types, size_t& index) {
  bool is_present = false;
  if (begin_field(name, is_present, types, index)) {
    if (is_present) {
      return true;
    } else {
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: mandatory key {} missing in field {}",
                             class_name, __func__, name, current_field_name());
      return false;
    }
  } else {
    return false;
  }
}

bool json_reader::begin_field(std::string_view name, bool& is_present,
                              span<const type_id_t> types, size_t& index) {
  SCOPE(position::object);
  field_.push_back(name);
  if (auto member = find_member(top<position::object>(), name);
      member != nullptr
      && member->val->data.index() != detail::json::value::null_index) {
    auto ft = field_type(top<position::object>(), name, field_type_suffix_);
    if (auto id = (*mapper_)(ft); id != invalid_type_id) {
      if (auto i = std::find(types.begin(), types.end(), id);
          i != types.end()) {
        index = static_cast<size_t>(std::distance(types.begin(), i));
        push(member->val);
        is_present = true;
        return true;
      }
    }
  }
  is_present = false;
  return true;
}

bool json_reader::end_field() {
  SCOPE(position::object);
  // Note: no pop() here, because the value(s) were already consumed. Only
  //       update the field_ for debugging.
  if (!field_.empty())
    field_.pop_back();
  return true;
}

bool json_reader::begin_tuple(size_t size) {
  size_t list_size = 0;
  if (begin_sequence(list_size)) {
    if (list_size == size) {
      return true;
    } else {
      err_ = format_to_error(
        sec::conversion_failed,
        "{}::{}: expected tuple of size {} in field {}, got a list of size {}",
        class_name, __func__, size, current_field_name(), list_size);
      return false;
    }
  } else {
    return false;
  }
}

bool json_reader::end_tuple() {
  return end_sequence();
}

bool json_reader::begin_key_value_pair() {
  SCOPE(position::members);
  if (auto& xs = top<position::members>(); !xs.at_end()) {
    auto& current = xs.current();
    push(current.val);
    push(current.key);
    return true;
  } else {
    err_ = format_to_error(
      sec::runtime_error,
      "{}::{}: tried reading a JSON::object sequentially past its end",
      class_name, __func__);
    return false;
  }
}

bool json_reader::end_key_value_pair() {
  SCOPE(position::members);
  top<position::members>().advance();
  return true;
}

bool json_reader::begin_sequence(size_t& size) {
  FN_DECL;
  return consume<false>(fn, [this, &size](const detail::json::value& val) {
    if (val.data.index() == detail::json::value::array_index) {
      auto& ls = std::get<detail::json::array>(val.data);
      size = ls.size();
      push(sequence{ls.begin(), ls.end()});
      return true;
    } else {
      err_ = format_to_error(
        sec::runtime_error,
        "{}::{}: expected type json::array, got {} in field {}", class_name, fn,
        type_name_from(val), current_field_name());
      return false;
    }
  });
}

bool json_reader::end_sequence() {
  SCOPE(position::sequence);
  if (top<position::sequence>().at_end()) {
    pop();
    // We called consume<false> at first, so we need to call it again with
    // <true> for advancing the position now.
    return consume<true>(__func__,
                         [](const detail::json::value&) { return true; });
  } else {
    err_ = format_to_error(
      sec::runtime_error,
      "{}::{}: failed to consume all elements from json::array", class_name,
      class_name, __func__);
    return false;
  }
}

bool json_reader::begin_associative_array(size_t& size) {
  FN_DECL;
  return consume<false>(fn, [this, &size](const detail::json::value& val) {
    if (val.data.index() == detail::json::value::object_index) {
      auto* obj = std::addressof(std::get<detail::json::object>(val.data));
      pop();
      size = obj->size();
      push(members{obj->begin(), obj->end()});
      return true;
    } else {
      err_ = format_to_error(
        sec::runtime_error,
        "{}::{}: expected type json::object, got {} in field {}", class_name,
        fn, type_name_from(val), current_field_name());
      return false;
    }
  });
}

bool json_reader::end_associative_array() {
  SCOPE(position::members);
  if (top<position::members>().at_end()) {
    pop();
    return true;
  } else {
    err_ = format_to_error(
      sec::runtime_error,
      "{}::{}: failed to consume all elements in an associative array",
      class_name, __func__);
    return false;
  }
}

bool json_reader::value(std::byte& x) {
  auto tmp = uint8_t{0};
  if (value(tmp)) {
    x = static_cast<std::byte>(tmp);
    return true;
  } else {
    return false;
  }
}

bool json_reader::value(bool& x) {
  FN_DECL;
  return consume<true>(fn, [this, &x](const detail::json::value& val) {
    if (val.data.index() == detail::json::value::bool_index) {
      x = std::get<bool>(val.data);
      return true;
    } else {
      err_ = format_to_error(
        sec::runtime_error,
        "{}::{}: expected type json::boolean, got {} in field {}", class_name,
        fn, type_name_from(val), current_field_name());
      return false;
    }
  });
}

template <bool PopOrAdvanceOnSuccess, class F>
bool json_reader::consume(const char* fn, F f) {
  auto current_pos = pos();
  switch (current_pos) {
    INVALID_AND_PAST_THE_END_CASES
    case position::value:
      if (f(*top<position::value>())) {
        if constexpr (PopOrAdvanceOnSuccess)
          pop();
        return true;
      } else {
        return false;
      }
    case position::key:
      if (f(detail::json::value{top<position::key>()})) {
        if constexpr (PopOrAdvanceOnSuccess)
          pop();
        return true;
      } else {
        return false;
      }
    case position::sequence:
      if (auto& ls = top<position::sequence>(); !ls.at_end()) {
        auto& curr = ls.current();
        if constexpr (PopOrAdvanceOnSuccess)
          ls.advance();
        return f(curr);
      } else {
        err_
          = format_to_error(sec::runtime_error,
                            "{}::{}: tried reading a json::array past the end",
                            class_name, fn);
        return false;
      }
    default:
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: expected type json::value, json::key, or "
                             "json::array, got {} in field {}",
                             class_name, fn, pretty_name(current_pos),
                             current_field_name());
      return false;
  }
}

template <class T>
bool json_reader::integer(T& x) {
  static constexpr const char* fn = "value";
  return consume<true>(fn, [this, &x](const detail::json::value& val) {
    if (val.data.index() == detail::json::value::integer_index) {
      auto i64 = std::get<int64_t>(val.data);
      if (detail::bounds_checker<T>::check(i64)) {
        x = static_cast<T>(i64);
        return true;
      }
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: integer out of bounds in field {}",
                             class_name, fn, current_field_name());
      return false;
    } else if (val.data.index() == detail::json::value::unsigned_index) {
      auto u64 = std::get<uint64_t>(val.data);
      if (detail::bounds_checker<T>::check(u64)) {
        x = static_cast<T>(u64);
        return true;
      }
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: integer out of bounds in field {}",
                             class_name, fn, current_field_name());
      return false;
    } else {
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: expected type json::integer, "
                             "got {} in field {}",
                             class_name, fn, type_name_from(val),
                             current_field_name());
      return false;
    }
  });
}

bool json_reader::value(int8_t& x) {
  return integer(x);
}

bool json_reader::value(uint8_t& x) {
  return integer(x);
}

bool json_reader::value(int16_t& x) {
  return integer(x);
}

bool json_reader::value(uint16_t& x) {
  return integer(x);
}

bool json_reader::value(int32_t& x) {
  return integer(x);
}

bool json_reader::value(uint32_t& x) {
  return integer(x);
}

bool json_reader::value(int64_t& x) {
  return integer(x);
}

bool json_reader::value(uint64_t& x) {
  return integer(x);
}

bool json_reader::value(float& x) {
  auto tmp = 0.0;
  if (value(tmp)) {
    x = static_cast<float>(tmp);
    return true;
  } else {
    return false;
  }
}

bool json_reader::value(double& x) {
  FN_DECL;
  return consume<true>(fn, [this, &x](const detail::json::value& val) {
    switch (val.data.index()) {
      case detail::json::value::double_index:
        x = std::get<double>(val.data);
        return true;
      case detail::json::value::integer_index:
        x = static_cast<double>(std::get<int64_t>(val.data));
        return true;
      case detail::json::value::unsigned_index:
        x = static_cast<double>(std::get<uint64_t>(val.data));
        return true;
      default:
        err_ = format_to_error(sec::runtime_error,
                               "{}::{}: expected type json::real, "
                               "got {} in field {}",
                               class_name, fn, type_name_from(val),
                               current_field_name());
        return false;
    }
  });
}

bool json_reader::value(long double& x) {
  auto tmp = 0.0;
  if (value(tmp)) {
    x = static_cast<long double>(tmp);
    return true;
  } else {
    return false;
  }
}

bool json_reader::value(std::string& x) {
  FN_DECL;
  return consume<true>(fn, [this, &x](const detail::json::value& val) {
    if (val.data.index() == detail::json::value::string_index) {
      x = std::get<std::string_view>(val.data);
      return true;
    } else {
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: expected type json::string, "
                             "got {} in field {}",
                             class_name, fn, type_name_from(val),
                             current_field_name());
      return false;
    }
  });
}

bool json_reader::value(std::u16string&) {
  err_ = format_to_error(sec::runtime_error,
                         "{}::{}: u16string support not implemented yet",
                         class_name, __func__);
  return false;
}

bool json_reader::value(std::u32string&) {
  err_ = format_to_error(sec::runtime_error,
                         "{}::{}: u32string support not implemented yet",
                         class_name, __func__);
  return false;
}

bool json_reader::value(span<std::byte>) {
  err_ = format_to_error(sec::runtime_error,
                         "{}::{}: byte span support not implemented yet",
                         class_name, __func__);
  return false;
}

json_reader::position json_reader::pos() const noexcept {
  if (st_ == nullptr)
    return position::invalid;
  else if (st_->empty())
    return position::past_the_end;
  else
    return static_cast<position>(st_->back().index());
}

void json_reader::append_current_field_name(std::string& result) {
  result += "ROOT";
  for (auto& key : field_) {
    result += '.';
    result.insert(result.end(), key.begin(), key.end());
  }
}

std::string json_reader::current_field_name() {
  std::string result;
  append_current_field_name(result);
  return result;
}

} // namespace caf
