// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/json_reader.hpp"

#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/print.hpp"
#include "caf/string_algorithms.hpp"

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

std::string type_clash(std::string_view want, std::string_view got) {
  std::string result = "type clash: expected ";
  result.insert(result.end(), want.begin(), want.end());
  result += ", got ";
  result.insert(result.end(), got.begin(), got.end());
  return result;
}

std::string type_clash(std::string_view want, caf::json_reader::position got) {
  return type_clash(want, pretty_name(got));
}

std::string type_clash(caf::json_reader::position want,
                       caf::json_reader::position got) {
  return type_clash(pretty_name(want), pretty_name(got));
}

std::string type_clash(std::string_view want,
                       const caf::detail::json::value& got) {
  using namespace std::literals;
  using caf::detail::json::value;
  switch (got.data.index()) {
    case value::integer_index:
      return type_clash(want, "json::integer"sv);
    case value::double_index:
      return type_clash(want, "json::real"sv);
    case value::bool_index:
      return type_clash(want, "json::boolean"sv);
    case value::string_index:
      return type_clash(want, "json::string"sv);
    case value::array_index:
      return type_clash(want, "json::array"sv);
    case value::object_index:
      return type_clash(want, "json::object"sv);
    default:
      return type_clash(want, "json::null"sv);
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
    emplace_error(sec::runtime_error, class_name, fn, current_field_name(),    \
                  "found an invalid position");                                \
    return false;                                                              \
  case position::past_the_end:                                                 \
    emplace_error(sec::runtime_error, class_name, fn, current_field_name(),    \
                  "tried reading past the end");                               \
    return false;

#define SCOPE(expected_position)                                               \
  if (auto got = pos(); got != expected_position) {                            \
    emplace_error(sec::runtime_error, class_name, __func__,                    \
                  current_field_name(), type_clash(expected_position, got));   \
    return false;                                                              \
  }

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

json_reader::json_reader() : super() {
  field_.reserve(8);
  has_human_readable_format_ = true;
}

json_reader::json_reader(actor_system& sys) : super(sys) {
  field_.reserve(8);
  has_human_readable_format_ = true;
}

json_reader::json_reader(execution_unit* ctx) : super(ctx) {
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
    set_error(make_error(ps));
    st_ = nullptr;
    return false;
  } else {
    err_.reset();
    detail::monotonic_buffer_resource::allocator<stack_type> alloc{&buf_};
    st_ = new (alloc.allocate(1)) stack_type(stack_allocator{&buf_});
    st_->reserve(16);
    st_->emplace_back(root_);
    return true;
  }
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
    if (auto id = query_type_id(type_name); id != invalid_type_id) {
      type = id;
      return true;
    } else {
      std::string what = "no type ID available for @type: ";
      what.insert(what.end(), type_name.begin(), type_name.end());
      emplace_error(sec::runtime_error, class_name, __func__,
                    current_field_name(), std::move(what));
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
        } else {
          emplace_error(sec::runtime_error, class_name, fn,
                        current_field_name(),
                        "expected a string argument to @type");
          return false;
        }
      } else {
        emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                      "found no @type member");
        return false;
      }
    } else {
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::object", val));
      return false;
    }
  });
}

bool json_reader::begin_object(type_id_t, std::string_view) {
  FN_DECL;
  return consume<false>(fn, [this](const detail::json::value& val) {
    if (val.data.index() == detail::json::value::object_index) {
      push(&std::get<detail::json::object>(val.data));
      return true;
    } else {
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::object", val));
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
      emplace_error(sec::runtime_error, class_name, __func__,
                    current_field_name(),
                    type_clash("json::value or json::array", current_pos));
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
    emplace_error(sec::runtime_error, class_name, __func__,
                  mandatory_field_missing_str(name));
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
      emplace_error(sec::runtime_error, class_name, __func__,
                    mandatory_field_missing_str(name));
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
    if (auto id = query_type_id(ft); id != invalid_type_id) {
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
      std::string msg;
      msg += "expected tuple of size ";
      detail::print(msg, size);
      msg += ", got a list of size ";
      detail::print(msg, list_size);
      emplace_error(sec::conversion_failed, class_name, __func__,
                    current_field_name(), std::move(msg));
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
    emplace_error(sec::runtime_error, class_name, __func__,
                  "tried reading a JSON::object sequentially past its end");
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
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::array", val));
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
    emplace_error(sec::runtime_error, class_name, __func__,
                  "failed to consume all elements from json::array");
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
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::object", val));
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
    emplace_error(sec::runtime_error, class_name, __func__,
                  "failed to consume all elements in an associative array");
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
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::boolean", val));
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
        emplace_error(sec::runtime_error, class_name, fn,
                      "tried reading a json::array past the end");
        return false;
      }
    default:
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::value", current_pos));
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
      } else {
        emplace_error(sec::runtime_error, class_name, fn,
                      "integer out of bounds");
        return false;
      }
    } else {
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::integer", val));
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
    if (val.data.index() == detail::json::value::double_index) {
      x = std::get<double>(val.data);
      return true;
    } else if (val.data.index() == detail::json::value::integer_index) {
      x = std::get<int64_t>(val.data);
      return true;
    } else {
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::real", val));
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
      detail::print_unescaped(x, std::get<std::string_view>(val.data));
      return true;
    } else {
      emplace_error(sec::runtime_error, class_name, fn, current_field_name(),
                    type_clash("json::string", val));
      return false;
    }
  });
}

bool json_reader::value(std::u16string&) {
  emplace_error(sec::runtime_error, class_name, __func__,
                "u16string support not implemented yet");
  return false;
}

bool json_reader::value(std::u32string&) {
  emplace_error(sec::runtime_error, class_name, __func__,
                "u32string support not implemented yet");
  return false;
}

bool json_reader::value(span<std::byte>) {
  emplace_error(sec::runtime_error, class_name, __func__,
                "byte span support not implemented yet");
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

std::string json_reader::mandatory_field_missing_str(std::string_view name) {
  std::string msg = "mandatory field '";
  append_current_field_name(msg);
  msg += '.';
  msg.insert(msg.end(), name.begin(), name.end());
  msg += "' missing";
  return msg;
}

} // namespace caf
