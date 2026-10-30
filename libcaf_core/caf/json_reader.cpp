// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_reader.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_handle_codec.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/default_actor_handle_codec.hpp"
#include "caf/detail/json.hpp"
#include "caf/format_to_error.hpp"
#include "caf/string_algorithms.hpp"

#include <fstream>
#include <memory_resource>
#include <variant>
#include <vector>

namespace caf::internal {

class json_reader_stack {
public:
  struct sequence {
    detail::json::array::const_iterator pos;

    detail::json::array::const_iterator end;

    bool at_end() const noexcept {
      return pos == end;
    }

    auto& current() {
      return *pos;
    }

    void advance() {
      ++pos;
    }
  };

  struct members {
    detail::json::object::const_iterator pos;

    detail::json::object::const_iterator end;

    bool at_end() const noexcept {
      return pos == end;
    }

    auto& current() {
      return *pos;
    }

    void advance() {
      ++pos;
    }
  };

  using json_key = std::string_view;

  using value_type
    = std::variant<const detail::json::value*, const detail::json::object*,
                   detail::json::null_t, json_key, sequence, members>;

  using allocator_type = std::pmr::polymorphic_allocator<value_type>;

  using data_type = std::vector<value_type, allocator_type>;

  explicit json_reader_stack(std::pmr::memory_resource* resource)
    : data(allocator_type{resource}), fields(resource) {
    // nop
  }

  data_type data;

  std::pmr::vector<std::string_view> fields;
};

} // namespace caf::internal

namespace {

static constexpr const char class_name[] = "caf::json_reader_base";

using sequence = caf::internal::json_reader_stack::sequence;

using members = caf::internal::json_reader_stack::members;

/// Denotes the type at the current position.
enum class position {
  value,
  object,
  null,
  key,
  sequence,
  members,
  past_the_end,
  invalid,
};

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
  auto i = std::ranges::find_if(*obj, [key](const auto& member) {
    return member.key == key;
  });
  return i != obj->end() ? &*i : nullptr;
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

const caf::detail::json::member*
find_type_member(const caf::detail::json::object& obj) {
  return find_member(&obj, "@type");
}

} // namespace

#define FN_DECL static constexpr const char* fn = __func__

#define INVALID_AND_PAST_THE_END_CASES                                         \
  case position::invalid:                                                      \
    err_ = format_to_error(sec::runtime_error,                                 \
                           "{}::{}: found an invalid position in field {}",    \
                           class_name, fn, util::current_field_name(*this));   \
    return false;                                                              \
  case position::past_the_end:                                                 \
    err_ = format_to_error(sec::runtime_error,                                 \
                           "{}::{}: unexpected end of input in field {}",      \
                           class_name, fn, util::current_field_name(*this));   \
    return false;

#define SCOPE(expected_position)                                               \
  if (auto got = util::pos(*this); got != expected_position) {                 \
    err_ = format_to_error(                                                    \
      sec::runtime_error, "{}::{}: expected type {}, got {} in field {}",      \
      class_name, __func__, util::pretty_name(expected_position),              \
      util::pretty_name(got), util::current_field_name(*this));                \
    return false;                                                              \
  }

namespace caf::internal {

class json_reader_util {
public:
  [[nodiscard]] static position pos(const json_reader_base& reader) noexcept {
    if (reader.st_ == nullptr)
      return position::invalid;
    if (reader.st_->data.empty())
      return position::past_the_end;
    return static_cast<position>(reader.st_->data.back().index());
  }

  static void append_current_field_name(const json_reader_base& reader,
                                        std::string& result) {
    result += "ROOT";
    if (reader.st_ != nullptr) {
      for (auto& key : reader.st_->fields) {
        result += '.';
        result.insert(result.end(), key.begin(), key.end());
      }
    }
  }

  static std::string current_field_name(const json_reader_base& reader) {
    std::string result;
    append_current_field_name(reader, result);
    return result;
  }

  template <bool PopOrAdvanceOnSuccess, class F>
  static bool consume(json_reader_base& reader, const char* fn, F f) {
    auto current_pos = pos(reader);
    switch (current_pos) {
      case position::invalid:
        reader.err_
          = format_to_error(sec::runtime_error,
                            "{}::{}: found an invalid position in field {}",
                            class_name, fn, current_field_name(reader));
        return false;
      case position::past_the_end:
        reader.err_
          = format_to_error(sec::runtime_error,
                            "{}::{}: unexpected end of input in field {}",
                            class_name, fn, current_field_name(reader));
        return false;
      case position::value:
        if (f(*top<position::value>(reader))) {
          if constexpr (PopOrAdvanceOnSuccess)
            pop(reader);
          return true;
        } else {
          return false;
        }
      case position::key:
        if (f(detail::json::value{top<position::key>(reader)})) {
          if constexpr (PopOrAdvanceOnSuccess)
            pop(reader);
          return true;
        } else {
          return false;
        }
      case position::sequence:
        if (auto& ls = top<position::sequence>(reader); !ls.at_end()) {
          auto& curr = ls.current();
          if constexpr (PopOrAdvanceOnSuccess)
            ls.advance();
          return f(curr);
        } else {
          reader.err_ = format_to_error(
            sec::runtime_error,
            "{}::{}: tried reading a json::array past the end", class_name, fn);
          return false;
        }
      default:
        reader.err_
          = format_to_error(sec::runtime_error,
                            "{}::{}: expected type json::value, json::key, or "
                            "json::array, got {} in field {}",
                            class_name, fn, pretty_name(current_pos),
                            current_field_name(reader));
        return false;
    }
  }

  template <class T>
  static bool integer(json_reader_base& reader, T& x) {
    static constexpr const char* fn = "value";
    return consume<true>(
      reader, fn, [&reader, &x](const detail::json::value& val) {
        if (val.data.index() == detail::json::value::integer_index) {
          auto i64 = std::get<int64_t>(val.data);
          if (detail::bounds_checker<T>::check(i64)) {
            x = static_cast<T>(i64);
            return true;
          }
          reader.err_
            = format_to_error(sec::runtime_error,
                              "{}::{}: integer out of bounds in field {}",
                              class_name, fn, current_field_name(reader));
          return false;
        } else if (val.data.index() == detail::json::value::unsigned_index) {
          auto u64 = std::get<uint64_t>(val.data);
          if (detail::bounds_checker<T>::check(u64)) {
            x = static_cast<T>(u64);
            return true;
          }
          reader.err_
            = format_to_error(sec::runtime_error,
                              "{}::{}: integer out of bounds in field {}",
                              class_name, fn, current_field_name(reader));
          return false;
        } else {
          reader.err_ = format_to_error(
            sec::runtime_error,
            "{}::{}: expected type json::integer, got {} in field {}",
            class_name, fn, type_name_from(val), current_field_name(reader));
          return false;
        }
      });
  }

  template <position P>
  static auto& top(json_reader_base& reader) noexcept {
    return std::get<static_cast<size_t>(P)>(reader.st_->data.back());
  }

  template <position P>
  static const auto& top(const json_reader_base& reader) noexcept {
    return std::get<static_cast<size_t>(P)>(reader.st_->data.back());
  }

  static void pop(json_reader_base& reader) {
    reader.st_->data.pop_back();
  }

  template <class T>
  static void push(json_reader_base& reader, T&& x) {
    reader.st_->data.emplace_back(std::forward<T>(x));
  }

  static std::string_view pretty_name(position pos) {
    switch (pos) {
      default:
        return "invalid input";
      case position::value:
        return "json::value";
      case position::object:
        return "json::object";
      case position::null:
        return "null";
      case position::key:
        return "json::key";
      case position::sequence:
        return "json::array";
      case position::members:
        return "json::members";
    }
  }
};

} // namespace caf::internal

namespace caf {

json_reader_base::json_reader_base() = default;

json_reader_base::json_reader_base(const type_id_mapper& mapper)
  : mapper_(&mapper) {
  // nop
}

json_reader_base::json_reader_base(const type_id_mapper& mapper,
                                   std::pmr::memory_resource* resource)
  : buf_(resource), mapper_(&mapper) {
  CAF_ASSERT(resource != nullptr);
}

json_reader_base::~json_reader_base() {
  // nop
}

std::string_view json_reader_base::field_type_suffix() const noexcept {
  return field_type_suffix_;
}

void json_reader_base::field_type_suffix(std::string_view suffix) noexcept {
  field_type_suffix_ = suffix;
}

void json_reader_base::mapper(const type_id_mapper* ptr) noexcept {
  mapper_ = ptr ? ptr : &get_default_type_id_mapper();
}

void json_reader_base::set_error(error stop_reason) {
  err_ = std::move(stop_reason);
}

error& json_reader_base::get_error() noexcept {
  return err_;
}

bool json_reader_base::load(std::string_view json_text) {
  reset();
  string_parser_state ps{json_text.begin(), json_text.end()};
  root_ = detail::json::parse_shallow(ps, &buf_);
  if (ps.code != pec::success) {
    err_ = ps.error();
    st_ = nullptr;
    return false;
  }
  err_.reset();
  std::pmr::polymorphic_allocator<internal::json_reader_stack> alloc{&buf_};
  st_ = new (alloc.allocate(1)) internal::json_reader_stack(&buf_);
  st_->data.reserve(16);
  st_->fields.reserve(8);
  st_->data.emplace_back(root_);
  return true;
}

bool json_reader_base::load_bytes(const_byte_span bytes) {
  return load(to_string_view(bytes));
}

bool json_reader_base::load_from(std::istream& input) {
  reset();
  using iterator_t = std::istreambuf_iterator<char>;
  detail::json::file_parser_state ps{iterator_t{input}, iterator_t{}};
  root_ = detail::json::parse(ps, &buf_);
  if (ps.code != pec::success) {
    err_ = ps.error();
    st_ = nullptr;
    return false;
  }
  err_.reset();
  std::pmr::polymorphic_allocator<internal::json_reader_stack> alloc{&buf_};
  st_ = new (alloc.allocate(1)) internal::json_reader_stack(&buf_);
  st_->data.reserve(16);
  st_->fields.reserve(8);
  st_->data.emplace_back(root_);
  return true;
}

bool json_reader_base::load_file(const char* path) {
  std::ifstream input{path};
  if (!input.is_open()) {
    err_ = sec::cannot_open_file;
    return false;
  }
  return load_from(input);
}

void json_reader_base::revert() {
  if (st_) {
    CAF_ASSERT(root_ != nullptr);
    err_.reset();
    st_->data.clear();
    st_->data.emplace_back(root_);
    st_->fields.clear();
  }
}

void json_reader_base::reset() {
  buf_.release();
  st_ = nullptr;
  err_.reset();
}

bool json_reader_base::has_human_readable_format() const noexcept {
  return true;
}

type_id_t json_reader_base::to_type_id(std::string_view name) const {
  return (*mapper_)(name);
}

bool json_reader_base::fetch_next_object_type(type_id_t& type) {
  std::string_view type_name;
  if (fetch_next_object_name(type_name)) {
    if (auto id = to_type_id(type_name); id != invalid_type_id) {
      type = id;
      return true;
    }
    err_ = format_to_error(
      sec::runtime_error,
      "{}::{}: no type ID available for @type: {} in field {}", class_name,
      __func__, type_name, util::current_field_name(*this));
  }
  return false;
}

bool json_reader_base::fetch_next_object_name(std::string_view& type_name) {
  FN_DECL;
  return util::consume<false>(
    *this, fn, [this, &type_name](const detail::json::value& val) {
      if (val.data.index() == detail::json::value::object_index) {
        const auto& obj = std::get<detail::json::object>(val.data);
        if (auto mem_ptr = find_type_member(obj)) {
          if (mem_ptr->val->data.index() == detail::json::value::string_index) {
            type_name = std::get<std::string_view>(mem_ptr->val->data);
            return true;
          }
          err_ = format_to_error(
            sec::runtime_error,
            "{}::{}: expected a string argument to @type in field {}",
            class_name, fn, util::current_field_name(*this));
          return false;
        }
        err_ = format_to_error(sec::runtime_error,
                               "{}::{}: found no @type member in field {}",
                               class_name, fn, util::current_field_name(*this));
        return false;
      }
      err_ = format_to_error(
        sec::runtime_error,
        "{}::{}: expected type json::object, got {} in field {}", class_name,
        fn, type_name_from(val), util::current_field_name(*this));
      return false;
    });
}

bool json_reader_base::begin_object(type_id_t, std::string_view) {
  FN_DECL;
  return util::consume<false>(
    *this, fn, [this](const detail::json::value& val) {
      if (val.data.index() == detail::json::value::object_index) {
        util::push(*this, &std::get<detail::json::object>(val.data));
        return true;
      } else {
        err_ = format_to_error(
          sec::runtime_error,
          "{}::{}: expected type json::object, got {} in field {}", class_name,
          fn, type_name_from(val), util::current_field_name(*this));
        return false;
      }
    });
}

bool json_reader_base::end_object() {
  FN_DECL;
  SCOPE(position::object);
  util::pop(*this);
  auto current_pos = util::pos(*this);
  switch (current_pos) {
    INVALID_AND_PAST_THE_END_CASES
    case position::value:
      util::pop(*this);
      return true;
    case position::sequence:
      util::top<position::sequence>(*this).advance();
      return true;
    default:
      err_ = format_to_error(sec::runtime_error,
                             "{}::{}: expected type json::value or "
                             "json::array, got {} in field {}",
                             class_name, fn, util::pretty_name(current_pos),
                             util::current_field_name(*this));
      return false;
  }
}

bool json_reader_base::begin_field(std::string_view name) {
  SCOPE(position::object);
  st_->fields.push_back(name);
  if (auto member = find_member(util::top<position::object>(*this), name)) {
    util::push(*this, member->val);
    return true;
  }
  err_ = format_to_error(sec::runtime_error,
                         "{}::{}: mandatory key {} missing in field {}",
                         class_name, __func__, name,
                         util::current_field_name(*this));
  return false;
}

bool json_reader_base::begin_field(std::string_view name, bool& is_present) {
  SCOPE(position::object);
  st_->fields.push_back(name);
  if (auto* member = find_member(util::top<position::object>(*this), name);
      member != nullptr
      && member->val->data.index() != detail::json::value::null_index) {
    util::push(*this, member->val);
    is_present = true;
    return true;
  }
  is_present = false;
  return true;
}

bool json_reader_base::begin_field(std::string_view name,
                                   std::span<const type_id_t> types,
                                   size_t& index) {
  bool is_present = false;
  if (begin_field(name, is_present, types, index)) {
    if (is_present) {
      return true;
    }
    err_ = format_to_error(sec::runtime_error,
                           "{}::{}: mandatory key {} missing in field {}",
                           class_name, __func__, name,
                           util::current_field_name(*this));
  }
  return false;
}

bool json_reader_base::begin_field(std::string_view name, bool& is_present,
                                   std::span<const type_id_t> types,
                                   size_t& index) {
  SCOPE(position::object);
  st_->fields.push_back(name);
  if (auto member = find_member(util::top<position::object>(*this), name);
      member != nullptr
      && member->val->data.index() != detail::json::value::null_index) {
    auto ft = field_type(util::top<position::object>(*this), name,
                         field_type_suffix_);
    if (auto id = to_type_id(ft); id != invalid_type_id) {
      if (auto i = std::ranges::find(types, id); i != types.end()) {
        index = static_cast<size_t>(std::distance(types.begin(), i));
        util::push(*this, member->val);
        is_present = true;
        return true;
      }
    }
  }
  is_present = false;
  return true;
}

bool json_reader_base::end_field() {
  SCOPE(position::object);
  // Note: no pop() here, because the value(s) were already consumed. Only
  //       update the field path for debugging.
  if (!st_->fields.empty())
    st_->fields.pop_back();
  return true;
}

bool json_reader_base::begin_tuple(size_t size) {
  size_t list_size = 0;
  if (begin_sequence(list_size)) {
    if (list_size == size) {
      return true;
    } else {
      err_ = format_to_error(sec::conversion_failed,
                             "{}::{}: expected tuple of size {} in field {}, "
                             "got a list of size {}",
                             class_name, __func__, size,
                             util::current_field_name(*this), list_size);
      return false;
    }
  } else {
    return false;
  }
}

bool json_reader_base::end_tuple() {
  return end_sequence();
}

bool json_reader_base::begin_key_value_pair() {
  SCOPE(position::members);
  if (auto& xs = util::top<position::members>(*this); !xs.at_end()) {
    auto& current = xs.current();
    util::push(*this, current.val);
    util::push(*this, current.key);
    return true;
  } else {
    err_ = format_to_error(
      sec::runtime_error,
      "{}::{}: tried reading a JSON::object sequentially past its end",
      class_name, __func__);
    return false;
  }
}

bool json_reader_base::end_key_value_pair() {
  SCOPE(position::members);
  util::top<position::members>(*this).advance();
  return true;
}

bool json_reader_base::begin_sequence(size_t& size) {
  FN_DECL;
  return util::consume<false>(
    *this, fn, [this, &size](const detail::json::value& val) {
      if (val.data.index() == detail::json::value::array_index) {
        auto& ls = std::get<detail::json::array>(val.data);
        size = ls.size();
        util::push(*this, sequence{ls.begin(), ls.end()});
        return true;
      } else {
        err_ = format_to_error(
          sec::runtime_error,
          "{}::{}: expected type json::array, got {} in field {}", class_name,
          fn, type_name_from(val), util::current_field_name(*this));
        return false;
      }
    });
}

bool json_reader_base::end_sequence() {
  SCOPE(position::sequence);
  if (util::top<position::sequence>(*this).at_end()) {
    util::pop(*this);
    // We called consume<false> at first, so we need to call it again with
    // <true> for advancing the position now.
    return util::consume<true>(*this, __func__,
                               [](const detail::json::value&) { return true; });
  } else {
    err_ = format_to_error(
      sec::runtime_error,
      "{}::{}: failed to consume all elements from json::array", class_name,
      class_name, __func__);
    return false;
  }
}

bool json_reader_base::begin_associative_array(size_t& size) {
  FN_DECL;
  return util::consume<false>(
    *this, fn, [this, &size](const detail::json::value& val) {
      if (val.data.index() == detail::json::value::object_index) {
        auto* obj = std::addressof(std::get<detail::json::object>(val.data));
        util::pop(*this);
        size = obj->size();
        util::push(*this, members{obj->begin(), obj->end()});
        return true;
      } else {
        err_ = format_to_error(
          sec::runtime_error,
          "{}::{}: expected type json::object, got {} in field {}", class_name,
          fn, type_name_from(val), util::current_field_name(*this));
        return false;
      }
    });
}

bool json_reader_base::end_associative_array() {
  SCOPE(position::members);
  if (util::top<position::members>(*this).at_end()) {
    util::pop(*this);
    return true;
  } else {
    err_ = format_to_error(
      sec::runtime_error,
      "{}::{}: failed to consume all elements in an associative array",
      class_name, __func__);
    return false;
  }
}

bool json_reader_base::value(std::byte& x) {
  auto tmp = uint8_t{0};
  if (value(tmp)) {
    x = static_cast<std::byte>(tmp);
    return true;
  } else {
    return false;
  }
}

bool json_reader_base::value(bool& x) {
  FN_DECL;
  return util::consume<true>(
    *this, fn, [this, &x](const detail::json::value& val) {
      if (val.data.index() == detail::json::value::bool_index) {
        x = std::get<bool>(val.data);
        return true;
      } else {
        err_ = format_to_error(
          sec::runtime_error,
          "{}::{}: expected type json::boolean, got {} in field {}", class_name,
          fn, type_name_from(val), util::current_field_name(*this));
        return false;
      }
    });
}

bool json_reader_base::value(int8_t& x) {
  return util::integer(*this, x);
}

bool json_reader_base::value(uint8_t& x) {
  return util::integer(*this, x);
}

bool json_reader_base::value(int16_t& x) {
  return util::integer(*this, x);
}

bool json_reader_base::value(uint16_t& x) {
  return util::integer(*this, x);
}

bool json_reader_base::value(int32_t& x) {
  return util::integer(*this, x);
}

bool json_reader_base::value(uint32_t& x) {
  return util::integer(*this, x);
}

bool json_reader_base::value(int64_t& x) {
  return util::integer(*this, x);
}

bool json_reader_base::value(uint64_t& x) {
  return util::integer(*this, x);
}

bool json_reader_base::value(float& x) {
  auto tmp = 0.0;
  if (value(tmp)) {
    x = static_cast<float>(tmp);
    return true;
  } else {
    return false;
  }
}

bool json_reader_base::value(double& x) {
  FN_DECL;
  return util::consume<true>(
    *this, fn, [this, &x](const detail::json::value& val) {
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
                                 util::current_field_name(*this));
          return false;
      }
    });
}

bool json_reader_base::value(long double& x) {
  auto tmp = 0.0;
  if (value(tmp)) {
    x = static_cast<long double>(tmp);
    return true;
  } else {
    return false;
  }
}

bool json_reader_base::value(std::string& x) {
  FN_DECL;
  return util::consume<true>(
    *this, fn, [this, &x](const detail::json::value& val) {
      if (val.data.index() == detail::json::value::string_index) {
        x = std::get<std::string_view>(val.data);
        return true;
      } else {
        err_ = format_to_error(sec::runtime_error,
                               "{}::{}: expected type json::string, "
                               "got {} in field {}",
                               class_name, fn, type_name_from(val),
                               util::current_field_name(*this));
        return false;
      }
    });
}

bool json_reader_base::value(std::u16string&) {
  err_ = format_to_error(sec::runtime_error,
                         "{}::{}: u16string support not implemented yet",
                         class_name, __func__);
  return false;
}

bool json_reader_base::value(std::u32string&) {
  err_ = format_to_error(sec::runtime_error,
                         "{}::{}: u32string support not implemented yet",
                         class_name, __func__);
  return false;
}

bool json_reader_base::value(byte_span) {
  err_ = format_to_error(sec::runtime_error,
                         "{}::{}: byte span support not implemented yet",
                         class_name, __func__);
  return false;
}

bool json_reader::value(strong_actor_ptr& ptr) {
  if (context_ != nullptr) {
    detail::default_actor_handle_codec codec{*context_};
    return codec.load(*this, ptr);
  }
  emplace_error(sec::no_context);
  return false;
}

bool json_reader_v2::value(strong_actor_ptr& ptr) {
  if (codec_ != nullptr)
    return codec_->load(*this, ptr);
  emplace_error(sec::no_actor_handle_codec);
  return false;
}

} // namespace caf
