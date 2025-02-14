// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/json_reader.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/json.hpp"
#include "caf/detail/print.hpp"
#include "caf/format_to_error.hpp"
#include "caf/internal/fast_pimpl.hpp"
#include "caf/string_algorithms.hpp"

#include <fstream>

namespace {

static constexpr const char class_name[] = "caf::json_reader";

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

namespace {

class impl : public byte_reader, public internal::fast_pimpl<impl> {
public:
  // -- member types -----------------------------------------------------------

  using super = byte_reader;

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

  using stack_allocator
    = detail::monotonic_buffer_resource::allocator<value_type>;

  using stack_type = std::vector<value_type, stack_allocator>;

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

  template <position P>
  auto& top() noexcept {
    return std::get<static_cast<size_t>(P)>(st_->back());
  }

  template <position P>
  const auto& top() const noexcept {
    return std::get<static_cast<size_t>(P)>(st_->back());
  }

  // -- constants --------------------------------------------------------------

  /// The value value for `field_type_suffix()`.
  static constexpr std::string_view field_type_suffix_default = "-type";

  // -- constructors, destructors, and assignment operators --------------------

  impl() {
    field_.reserve(8);
  }

  explicit impl(actor_system& sys) : sys_(&sys) {
    field_.reserve(8);
  }

  impl(const json_reader&) = delete;

  impl& operator=(const json_reader&) = delete;

  ~impl() override {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Returns the suffix for generating type annotation fields for variant
  /// fields. For example, CAF inserts field called "@foo${field_type_suffix}"
  /// for a variant field called "foo".
  [[nodiscard]] std::string_view field_type_suffix() const noexcept {
    return field_type_suffix_;
  }

  /// Configures whether the writer omits empty fields.
  void field_type_suffix(std::string_view suffix) noexcept {
    field_type_suffix_ = suffix;
  }

  /// Returns the type ID mapper used by the writer.
  [[nodiscard]] const type_id_mapper* mapper() const noexcept {
    return mapper_;
  }

  /// Changes the type ID mapper for the writer.
  void mapper(const type_id_mapper* ptr) noexcept {
    mapper_ = ptr;
  }

  // -- modifiers --------------------------------------------------------------

  void set_error(error stop_reason) override {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept override {
    return err_;
  }

  /// Parses @p json_text into an internal representation. After loading the
  /// JSON input, the reader is ready for attempting to deserialize inspectable
  /// objects.
  /// @warning The internal data structure keeps pointers into @p json_text.
  ///          Hence, the buffer pointed to by the string view must remain valid
  ///          until either destroying this reader or calling `reset`.
  /// @note Implicitly calls `reset`.
  bool load(std::string_view json_text) {
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

  bool load_bytes(span<const std::byte> bytes) override {
    auto utf8 = std::string_view{reinterpret_cast<const char*>(bytes.data()),
                                 bytes.size()};
    return load(utf8);
  }

  /// Reads the input stream @p input and parses the content into an internal
  /// representation. After loading the JSON input, the reader is ready for
  /// attempting to deserialize inspectable objects.
  /// @note Implicitly calls `reset`.
  bool load_from(std::istream& input) {
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
    detail::monotonic_buffer_resource::allocator<stack_type> alloc{&buf_};
    st_ = new (alloc.allocate(1)) stack_type(stack_allocator{&buf_});
    st_->reserve(16);
    st_->emplace_back(root_);
    return true;
  }

  /// Parses the content of the file under the given @p path. After loading the
  /// content of the JSON file, the reader is ready for attempting to
  /// deserialize inspectable objects.
  /// @note Implicitly calls `reset`.
  bool load_file(const char* path) {
    std::ifstream input{path};
    if (!input.is_open()) {
      err_ = sec::cannot_open_file;
      return false;
    }
    return load_from(input);
  }

  /// @copydoc load_file
  bool load_file(const std::string& path) {
    return load_file(path.c_str());
  }

  /// Reverts the state of the reader back to where it was after calling `load`.
  /// @post The reader is ready for attempting to deserialize another
  ///       inspectable object.
  void revert() {
    if (st_) {
      CAF_ASSERT(root_ != nullptr);
      err_.reset();
      st_->clear();
      st_->emplace_back(root_);
      field_.clear();
    }
  }

  /// Removes any loaded JSON data and reclaims memory resources.
  void reset() {
    buf_.reclaim();
    st_ = nullptr;
    err_.reset();
    field_.clear();
  }

  // -- overrides --------------------------------------------------------------

  caf::actor_system* sys() const noexcept override {
    return sys_;
  }

  bool has_human_readable_format() const noexcept override {
    return true;
  }

  bool fetch_next_object_type(type_id_t& type) override {
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

  bool fetch_next_object_name(std::string_view& type_name) override {
    FN_DECL;
    return consume<false>(fn, [this,
                               &type_name](const detail::json::value& val) {
      if (val.data.index() == detail::json::value::object_index) {
        auto& obj = std::get<detail::json::object>(val.data);
        if (auto mem_ptr = find_member(&obj, "@type")) {
          if (mem_ptr->val->data.index() == detail::json::value::string_index) {
            type_name = std::get<std::string_view>(mem_ptr->val->data);
            return true;
          }
          err_ = format_to_error(
            sec::runtime_error,
            "{}::{}: expected a string argument to @type in field {}",
            class_name, fn, current_field_name());
          return false;
        }
        err_ = format_to_error(sec::runtime_error, class_name, fn,
                               current_field_name(), "found no @type member");
        return false;
      }
      err_ = format_to_error(
        sec::runtime_error,
        "{}::{}: expected type json::object, got {} in field {}", class_name,
        fn, type_name_from(val), current_field_name());
      return false;
    });
  }

  bool begin_object(type_id_t, std::string_view) override {
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

  bool end_object() override {
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
        err_ = format_to_error(sec::runtime_error,
                               "{}::{}: expected type json::value or "
                               "json::array, got {} in field {}",
                               class_name, fn, pretty_name(current_pos),
                               current_field_name());
        return false;
    }
  }

  bool begin_field(std::string_view name) override {
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

  bool begin_field(std::string_view name, bool& is_present) override {
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

  bool begin_field(std::string_view name, span<const type_id_t> types,
                   size_t& index) override {
    bool is_present = false;
    if (begin_field(name, is_present, types, index)) {
      if (is_present) {
        return true;
      } else {
        err_ = format_to_error(sec::runtime_error,
                               "{}::{}: mandatory key {} missing in field {}",
                               class_name, __func__, name,
                               current_field_name());
        return false;
      }
    } else {
      return false;
    }
  }

  bool begin_field(std::string_view name, bool& is_present,
                   span<const type_id_t> types, size_t& index) override {
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

  bool end_field() override {
    SCOPE(position::object);
    // Note: no pop() here, because the value(s) were already consumed. Only
    //       update the field_ for debugging.
    if (!field_.empty())
      field_.pop_back();
    return true;
  }

  bool begin_tuple(size_t size) override {
    size_t list_size = 0;
    if (begin_sequence(list_size)) {
      if (list_size == size) {
        return true;
      } else {
        err_ = format_to_error(sec::conversion_failed,
                               "{}::{}: expected tuple of size {} in field {}, "
                               "got a list of size {}",
                               class_name, __func__, size, current_field_name(),
                               list_size);
        return false;
      }
    } else {
      return false;
    }
  }

  bool end_tuple() override {
    return end_sequence();
  }

  bool begin_key_value_pair() override {
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

  bool end_key_value_pair() override {
    SCOPE(position::members);
    top<position::members>().advance();
    return true;
  }

  bool begin_sequence(size_t& size) override {
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
          "{}::{}: expected type json::array, got {} in field {}", class_name,
          fn, type_name_from(val), current_field_name());
        return false;
      }
    });
  }

  bool end_sequence() override {
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

  bool begin_associative_array(size_t& size) override {
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

  bool end_associative_array() override {
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

  bool value(std::byte& x) override {
    auto tmp = uint8_t{0};
    if (value(tmp)) {
      x = static_cast<std::byte>(tmp);
      return true;
    } else {
      return false;
    }
  }

  bool value(bool& x) override {
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

  bool value(int8_t& x) override {
    return integer(x);
  }

  bool value(uint8_t& x) override {
    return integer(x);
  }

  bool value(int16_t& x) override {
    return integer(x);
  }

  bool value(uint16_t& x) override {
    return integer(x);
  }

  bool value(int32_t& x) override {
    return integer(x);
  }

  bool value(uint32_t& x) override {
    return integer(x);
  }

  bool value(int64_t& x) override {
    return integer(x);
  }

  bool value(uint64_t& x) override {
    return integer(x);
  }

  bool value(float& x) override {
    auto tmp = 0.0;
    if (value(tmp)) {
      x = static_cast<float>(tmp);
      return true;
    } else {
      return false;
    }
  }

  bool value(double& x) override {
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

  bool value(long double& x) override {
    auto tmp = 0.0;
    if (value(tmp)) {
      x = static_cast<long double>(tmp);
      return true;
    } else {
      return false;
    }
  }

  bool value(std::string& x) override {
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

  bool value(std::u16string&) override {
    err_ = format_to_error(sec::runtime_error,
                           "{}::{}: u16string support not implemented yet",
                           class_name, __func__);
    return false;
  }

  bool value(std::u32string&) override {
    err_ = format_to_error(sec::runtime_error,
                           "{}::{}: u32string support not implemented yet",
                           class_name, __func__);
    return false;
  }

  bool value(span<std::byte>) override {
    err_ = format_to_error(sec::runtime_error,
                           "{}::{}: byte span support not implemented yet",
                           class_name, __func__);
    return false;
  }

private:
  [[nodiscard]] position pos() const noexcept {
    if (st_ == nullptr)
      return position::invalid;
    else if (st_->empty())
      return position::past_the_end;
    else
      return static_cast<position>(st_->back().index());
  }

  void append_current_field_name(std::string& result) {
    result += "ROOT";
    for (auto& key : field_) {
      result += '.';
      result.insert(result.end(), key.begin(), key.end());
    }
  }

  std::string current_field_name() {
    std::string result;
    append_current_field_name(result);
    return result;
  }

  template <bool PopOrAdvanceOnSuccess, class F>
  bool consume(const char* fn, F f) {
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
          err_ = format_to_error(
            sec::runtime_error,
            "{}::{}: tried reading a json::array past the end", class_name, fn);
          return false;
        }
      default:
        err_
          = format_to_error(sec::runtime_error,
                            "{}::{}: expected type json::value, json::key, or "
                            "json::array, got {} in field {}",
                            class_name, fn, pretty_name(current_pos),
                            current_field_name());
        return false;
    }
  }

  template <class T>
  bool integer(T& x) {
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

  void pop() {
    st_->pop_back();
  }

  template <class T>
  void push(T&& x) {
    st_->emplace_back(std::forward<T>(x));
  }

  std::string_view pretty_name(position pos) {
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

  actor_system* sys_ = nullptr;

  detail::monotonic_buffer_resource buf_;

  stack_type* st_ = nullptr;

  detail::json::value* root_ = nullptr;

  std::string_view field_type_suffix_ = field_type_suffix_default;

  /// Keeps track of the current field for better debugging output.
  std::vector<std::string_view> field_;

  /// The mapper implementation we use by default.
  default_type_id_mapper default_mapper_;

  /// Configures which ID mapper we use to translate between type IDs and names.
  const type_id_mapper* mapper_ = &default_mapper_;

  error err_;
};

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

json_reader::json_reader() {
  impl::construct(impl_);
}

json_reader::json_reader(actor_system& sys) {
  impl::construct(impl_, sys);
}

json_reader::~json_reader() {
  impl::destruct(impl_);
}

// -- properties -------------------------------------------------------------

[[nodiscard]] std::string_view json_reader::field_type_suffix() const noexcept {
  return impl::cast(impl_).field_type_suffix();
}

void json_reader::field_type_suffix(std::string_view suffix) noexcept {
  impl::cast(impl_).field_type_suffix(suffix);
}

[[nodiscard]] const type_id_mapper* json_reader::mapper() const noexcept {
  return impl::cast(impl_).mapper();
}

/// Changes the type ID mapper for the writer.
void json_reader::mapper(const type_id_mapper* ptr) noexcept {
  impl::cast(impl_).mapper(ptr);
}

// -- modifiers --------------------------------------------------------------

void json_reader::set_error(error stop_reason) {
  impl::cast(impl_).set_error(std::move(stop_reason));
}

error& json_reader::get_error() noexcept {
  return impl::cast(impl_).get_error();
}

bool json_reader::load(std::string_view json_text) {
  return impl::cast(impl_).load(json_text);
}

bool json_reader::load_bytes(span<const std::byte> bytes) {
  return impl::cast(impl_).load_bytes(bytes);
}

bool json_reader::load_from(std::istream& input) {
  return impl::cast(impl_).load_from(input);
}

bool json_reader::load_file(const char* path) {
  return impl::cast(impl_).load_file(path);
}

bool json_reader::load_file(const std::string& path) {
  return impl::cast(impl_).load_file(path);
}

void json_reader::revert() {
  impl::cast(impl_).revert();
}

void json_reader::reset() {
  impl::cast(impl_).reset();
}

// -- interface functions ------------------------------------------------------

caf::actor_system* json_reader::sys() const noexcept {
  return impl::cast(impl_).sys();
}

bool json_reader::has_human_readable_format() const noexcept {
  return impl::cast(impl_).has_human_readable_format();
}

bool json_reader::fetch_next_object_type(type_id_t& type) {
  return impl::cast(impl_).fetch_next_object_type(type);
}

bool json_reader::fetch_next_object_name(std::string_view& type_name) {
  return impl::cast(impl_).fetch_next_object_name(type_name);
}

bool json_reader::begin_object(type_id_t, std::string_view) {
  return impl::cast(impl_).begin_object(type_id_t{}, std::string_view{});
}

bool json_reader::end_object() {
  return impl::cast(impl_).end_object();
}

bool json_reader::begin_field(std::string_view name) {
  return impl::cast(impl_).begin_field(name);
}

bool json_reader::begin_field(std::string_view name, bool& is_present) {
  return impl::cast(impl_).begin_field(name, is_present);
}

bool json_reader::begin_field(std::string_view name,
                              span<const type_id_t> types, size_t& index) {
  return impl::cast(impl_).begin_field(name, types, index);
}

bool json_reader::begin_field(std::string_view name, bool& is_present,
                              span<const type_id_t> types, size_t& index) {
  return impl::cast(impl_).begin_field(name, is_present, types, index);
}

bool json_reader::end_field() {
  return impl::cast(impl_).end_field();
}

bool json_reader::begin_tuple(size_t size) {
  return impl::cast(impl_).begin_tuple(size);
}

bool json_reader::end_tuple() {
  return impl::cast(impl_).end_tuple();
}

bool json_reader::begin_key_value_pair() {
  return impl::cast(impl_).begin_key_value_pair();
}

bool json_reader::end_key_value_pair() {
  return impl::cast(impl_).end_key_value_pair();
}

bool json_reader::begin_sequence(size_t& size) {
  return impl::cast(impl_).begin_sequence(size);
}

bool json_reader::end_sequence() {
  return impl::cast(impl_).end_sequence();
}

bool json_reader::begin_associative_array(size_t& size) {
  return impl::cast(impl_).begin_associative_array(size);
}

bool json_reader::end_associative_array() {
  return impl::cast(impl_).end_associative_array();
}

bool json_reader::value(std::byte& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(bool& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(int8_t& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(uint8_t& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(int16_t& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(uint16_t& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(int32_t& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(uint32_t& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(int64_t& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(uint64_t& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(float& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(double& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(long double& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(std::string& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(std::u16string& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(std::u32string& x) {
  return impl::cast(impl_).value(x);
}

bool json_reader::value(span<std::byte> x) {
  return impl::cast(impl_).value(x);
}

} // namespace caf
