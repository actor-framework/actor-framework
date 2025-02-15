// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/config_value_reader.hpp"

#include "caf/config_value.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/overload.hpp"
#include "caf/detail/parse.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/print.hpp"
#include "caf/format_to_error.hpp"
#include "caf/internal/fast_pimpl.hpp"
#include "caf/settings.hpp"

#include <memory>
#include <stack>
#include <vector>

namespace caf {

namespace {

class impl : public deserializer, public internal::fast_pimpl<impl> {
public:
  // -- member types------------------------------------------------------------

  using super = deserializer;

  using key_ptr = const std::string*;

  struct absent_field {};

  struct sequence {
    using list_pointer = const std::vector<config_value>*;
    size_t index;
    list_pointer ls;
    explicit sequence(list_pointer ls) : index(0), ls(ls) {
      // nop
    }
    bool at_end() const noexcept {
      return index >= ls->size();
    }
    const config_value& current() {
      return (*ls)[index];
    }
    void advance() {
      ++index;
    }
  };

  struct associative_array {
    settings::const_iterator pos;
    settings::const_iterator end;
    bool at_end() const noexcept {
      return pos == end;
    }
    const std::pair<const std::string, config_value>& current() {
      return *pos;
    }
  };

  using value_type = std::variant<const settings*, const config_value*, key_ptr,
                                  absent_field, sequence, associative_array>;

  using stack_type = std::stack<value_type, std::vector<value_type>>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit impl(const config_value* input) {
    st_.push(input);
  }

  impl(const config_value* input, actor_system& sys) : sys_(&sys) {
    st_.push(input);
  }

  impl(const impl&) = delete;

  impl& operator=(const impl&) = delete;

  // -- stack access -----------------------------------------------------------

  value_type& top() {
    return st_.top();
  }

  void pop() {
    return st_.pop();
  }

  void set_error(error stop_reason) {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept {
    return err_;
  }

  caf::actor_system* sys() const noexcept {
    return sys_;
  }

  bool has_human_readable_format() const noexcept {
    return true;
  }

  bool fetch_next_object_type(type_id_t& type);

  bool begin_object(type_id_t type, std::string_view);

  bool end_object();

  bool begin_field(std::string_view name);

  bool begin_field(std::string_view name, bool& is_present);

  bool begin_field(std::string_view name, span<const type_id_t> types,
                   size_t& index);

  bool begin_field(std::string_view name, bool& is_present,
                   span<const type_id_t> types, size_t& index);

  bool end_field();

  bool begin_tuple(size_t size);

  bool end_tuple();

  bool begin_key_value_pair();

  bool end_key_value_pair();

  bool begin_sequence(size_t& size);

  bool end_sequence();

  bool begin_associative_array(size_t& size);

  bool end_associative_array();

  bool value(std::byte& x);

  template <class T>
  bool pull(impl& reader, T& x) {
    using internal_type
      = std::conditional_t<std::is_floating_point_v<T>, config_value::real, T>;
    auto assign = [&x](auto& result) {
      if constexpr (std::is_floating_point_v<T>) {
        x = static_cast<T>(result);
      } else {
        x = result;
      }
    };
    auto& top = reader.top();
    if (holds_alternative<const config_value*>(top)) {
      auto ptr = get<const config_value*>(top);
      if (auto val = get_as<internal_type>(*ptr)) {
        assign(*val);
        reader.pop();
        return true;
      } else {
        reader.set_error(std::move(val.error()));
        return false;
      }
    } else if (holds_alternative<sequence>(top)) {
      auto& seq = get<sequence>(top);
      if (seq.at_end()) {
        reader.emplace_error(sec::runtime_error,
                             "value: sequence out of bounds");
        return false;
      }
      auto ptr = std::addressof(seq.current());
      if (auto val = get_as<internal_type>(*ptr)) {
        assign(*val);
        seq.advance();
        return true;
      } else {
        reader.set_error(std::move(val.error()));
        return false;
      }
    } else if (holds_alternative<key_ptr>(top)) {
      auto ptr = get<key_ptr>(top);
      if constexpr (std::is_same_v<std::string, T>) {
        x = *ptr;
        reader.pop();
        return true;
      } else {
        if (auto err = detail::parse(*ptr, x)) {
          reader.set_error(std::move(err));
          return false;
        } else {
          return true;
        }
      }
    }
    reader.emplace_error(sec::conversion_failed,
                         "expected a value, sequence, or key");
    return false;
  }

  bool value(bool& x);

  bool value(int8_t& x);

  bool value(uint8_t& x);

  bool value(int16_t& x);

  bool value(uint16_t& x);

  bool value(int32_t& x);

  bool value(uint32_t& x);

  bool value(int64_t& x);

  bool value(uint64_t& x);

  bool value(float& x);

  bool value(double& x);

  bool value(long double& x);

  bool value(std::string& x);

  bool value(std::u16string&);

  bool value(std::u32string&);

  bool value(span<std::byte> bytes);

private:
  // Sets `type` according to the `@type` field in `obj` or to the type ID of
  // `settings` as fallback if no such field exists.
  bool fetch_object_type(const settings* obj, type_id_t& type);

  actor_system* sys_ = nullptr;

  stack_type st_;

  // Stores on-the-fly converted values.
  std::vector<std::unique_ptr<config_value>> scratch_space_;

  error err_;
};

template <class T>
struct pretty_name;

#define PRETTY_NAME(type, pretty_str)                                          \
  template <>                                                                  \
  struct pretty_name<type> {                                                   \
    [[maybe_unused]] static constexpr const char* value = pretty_str;          \
  }

PRETTY_NAME(const caf::settings*, "dictionary");
PRETTY_NAME(const caf::config_value*, "config_value");
PRETTY_NAME(const std::string*, "key");
PRETTY_NAME(impl::absent_field, "absent field");
PRETTY_NAME(impl::sequence, "sequence");
PRETTY_NAME(impl::associative_array, "associative array");

template <class T>
inline constexpr auto pretty_name_v = pretty_name<T>::value;

auto get_pretty_name(const impl::value_type& x) {
  const char* pretty_names[] = {
    "dictionary",   "config_value", "key",
    "absent field", "sequence",     "associative array",
  };
  return pretty_names[x.index()];
}

#define CHECK_NOT_EMPTY()                                                      \
  do {                                                                         \
    if (st_.empty()) {                                                         \
      emplace_error(sec::runtime_error, "mismatching calls to begin/end");     \
      return false;                                                            \
    }                                                                          \
  } while (false)

#define SCOPE(top_type)                                                        \
  CHECK_NOT_EMPTY();                                                           \
  if (!holds_alternative<top_type>(st_.top())) {                               \
    std::string msg;                                                           \
    msg += "type clash in function ";                                          \
    msg += __func__;                                                           \
    msg += ": expected ";                                                      \
    msg += pretty_name_v<top_type>;                                            \
    msg += " got ";                                                            \
    msg += get_pretty_name(st_.top());                                         \
    emplace_error(sec::runtime_error, std::move(msg));                         \
    return false;                                                              \
  }                                                                            \
  [[maybe_unused]] auto& top = get<top_type>(st_.top());

bool impl::fetch_next_object_type(type_id_t& type) {
  if (st_.empty()) {
    emplace_error(sec::runtime_error,
                  "tried to read multiple objects from the root object");
    return false;
  } else {
    auto f = detail::make_overload(
      [this](const settings*) {
        emplace_error(sec::runtime_error,
                      "fetch_next_object_type called inside an object");
        return false;
      },
      [this, &type](const config_value* val) {
        auto tid = val->type_id();
        if (tid != type_id_v<config_value::dictionary>) {
          type = tid;
          return true;
        } else {
          return fetch_object_type(get_if<settings>(val), type);
        }
      },
      [this](key_ptr) {
        emplace_error(
          sec::runtime_error,
          "reading an object from a dictionary key not implemented yet");
        return false;
      },
      [this](absent_field) {
        emplace_error(
          sec::runtime_error,
          "fetch_next_object_type called inside non-existent optional field");
        return false;
      },
      [this, &type](sequence& seq) {
        if (seq.at_end()) {
          emplace_error(sec::runtime_error, "list index out of bounds");
          return false;
        }
        auto& val = seq.current();
        auto tid = val.type_id();
        if (tid != type_id_v<config_value::dictionary>) {
          type = tid;
          return true;
        } else {
          return fetch_object_type(get_if<settings>(&val), type);
        }
      },
      [this](associative_array&) {
        emplace_error(sec::runtime_error,
                      "fetch_next_object_type called inside associative array");
        return false;
      });
    return visit(f, st_.top());
  }
}

bool impl::begin_object(type_id_t type, std::string_view) {
  if (st_.empty()) {
    emplace_error(sec::runtime_error,
                  "tried to read multiple objects from the root object");
    return false;
  }
  auto f = detail::make_overload(
    [this](const settings*) {
      emplace_error(sec::runtime_error,
                    "begin_object called inside another object");
      return false;
    },
    [this](const config_value* val) {
      if (auto obj = get_if<settings>(val)) {
        // Unbox the dictionary.
        st_.top() = obj;
        return true;
      } else if (auto dict = val->to_dictionary()) {
        // Replace the actual config value on the stack with the on-the-fly
        // converted dictionary.
        auto ptr = std::make_unique<config_value>(std::move(*dict));
        const settings* unboxed = std::addressof(get<settings>(*ptr));
        st_.top() = unboxed;
        scratch_space_.emplace_back(std::move(ptr));
        return true;
      } else {
        emplace_error(sec::conversion_failed, "cannot read input as object");
        return false;
      }
    },
    [this](key_ptr) {
      emplace_error(
        sec::runtime_error,
        "reading an object from a dictionary key not implemented yet");
      return false;
    },
    [this](absent_field) {
      emplace_error(sec::runtime_error,
                    "begin_object called inside non-existent optional field");
      return false;
    },
    [this](sequence& seq) {
      if (seq.at_end()) {
        emplace_error(sec::runtime_error,
                      "begin_object: sequence out of bounds");
        return false;
      }
      if (auto obj = get_if<settings>(std::addressof(seq.current()))) {
        seq.advance();
        st_.push(obj);
        return true;
      } else {
        emplace_error(sec::conversion_failed, "cannot read input as object");
        return false;
      }
    },
    [this](associative_array&) {
      emplace_error(sec::runtime_error,
                    "fetch_next_object_type called inside associative array");
      return false;
    });
  if (visit(f, st_.top())) {
    // Perform a type check if type is a valid ID and the object contains an
    // "@type" field.
    if (type != invalid_type_id) {
      CAF_ASSERT(holds_alternative<const settings*>(st_.top()));
      auto obj = get<const settings*>(st_.top());
      auto want = query_type_name(type);
      if (auto i = obj->find("@type"); i != obj->end()) {
        if (auto got = get_if<std::string>(std::addressof(i->second))) {
          if (want != *got) {
            err_ = format_to_error(sec::type_clash,
                                   "expected type: {}, found type: {}", want,
                                   *got);
            return false;
          }
        }
      }
    }
    return true;
  } else {
    return false;
  }
}

bool impl::end_object() {
  SCOPE(const settings*);
  st_.pop();
  return true;
}

bool impl::begin_field(std::string_view name) {
  SCOPE(const settings*);
  if (auto i = top->find(name); i != top->end()) {
    st_.push(std::addressof(i->second));
    return true;
  } else {
    emplace_error(sec::runtime_error, "no such field: " + std::string{name});
    return false;
  }
}

bool impl::begin_field(std::string_view name, bool& is_present) {
  SCOPE(const settings*);
  if (auto i = top->find(name); i != top->end()) {
    is_present = true;
    st_.push(std::addressof(i->second));
  } else {
    is_present = false;
  }
  return true;
}

bool impl::begin_field(std::string_view name, span<const type_id_t> types,
                       size_t& index) {
  SCOPE(const settings*);
  std::string key;
  key += '@';
  key.insert(key.end(), name.begin(), name.end());
  key += "-type";
  type_id_t id = 0;
  if (auto str = get_if<std::string>(top, key); !str) {
    emplace_error(sec::runtime_error, "could not find type annotation: " + key);
    return false;
  } else if (id = query_type_id(*str); id == invalid_type_id) {
    emplace_error(sec::runtime_error, "no such type: " + *str);
    return false;
  } else if (auto i = std::find(types.begin(), types.end(), id);
             i == types.end()) {
    emplace_error(sec::conversion_failed,
                  "instrid type for variant field: " + *str);
    return false;
  } else {
    index = static_cast<size_t>(std::distance(types.begin(), i));
  }
  return begin_field(name);
}

bool impl::begin_field(std::string_view name, bool& is_present,
                       span<const type_id_t> types, size_t& index) {
  SCOPE(const settings*);
  if (top->contains(name)) {
    is_present = true;
    return begin_field(name, types, index);
  } else {
    is_present = false;
    return true;
  }
}

bool impl::end_field() {
  CHECK_NOT_EMPTY();
  // Note: no pop() here, because the value(s) were already consumed.
  return true;
}

bool impl::begin_tuple(size_t size) {
  size_t list_size = 0;
  if (begin_sequence(list_size)) {
    if (list_size == size)
      return true;
    std::string msg;
    msg += "expected tuple of size ";
    detail::print(msg, size);
    msg += ", got tuple of size ";
    detail::print(msg, list_size);
    emplace_error(sec::conversion_failed, std::move(msg));
    return false;
  }
  return false;
}

bool impl::end_tuple() {
  return end_sequence();
}

bool impl::begin_key_value_pair() {
  SCOPE(associative_array);
  if (top.at_end()) {
    emplace_error(sec::runtime_error,
                  "tried to read associate array past its end");
    return false;
  }
  auto& kvp = top.current();
  st_.push(std::addressof(kvp.second));
  st_.push(std::addressof(kvp.first));
  return true;
}

bool impl::end_key_value_pair() {
  SCOPE(associative_array);
  ++top.pos;
  return true;
}

bool impl::begin_sequence(size_t& size) {
  SCOPE(const config_value*);
  if (auto ls = get_if<config_value::list>(top)) {
    size = ls->size();
    // "Transform" the top element to a list. Otherwise, we would need some
    // extra logic only to clean up the object.
    st_.top() = sequence{ls};
    return true;
  }
  std::string msg = "expected a list, got a ";
  msg += top->type_name();
  emplace_error(sec::conversion_failed, std::move(msg));
  return false;
}

bool impl::end_sequence() {
  SCOPE(sequence);
  if (!top.at_end()) {
    emplace_error(sec::runtime_error,
                  "failed to consume all elements in a sequence");
    return false;
  }
  st_.pop();
  return true;
}

bool impl::begin_associative_array(size_t& size) {
  SCOPE(const config_value*);
  if (auto dict = get_if<settings>(top)) {
    size = dict->size();
    // Morph top object, it's being "consumed" by begin_.../end_....
    st_.top() = associative_array{dict->begin(), dict->end()};
    return true;
  }
  std::string msg = "begin_associative_array: expected a dictionary, got a ";
  msg += top->type_name();
  emplace_error(sec::conversion_failed, std::move(msg));
  return false;
}

bool impl::end_associative_array() {
  SCOPE(associative_array);
  if (!top.at_end()) {
    emplace_error(sec::runtime_error,
                  "failed to consume all elements in an associative array");
    return false;
  }
  st_.pop();
  return true;
}

bool impl::value(std::byte& x) {
  CHECK_NOT_EMPTY();
  auto tmp = uint8_t{0};
  if (pull(*this, tmp)) {
    x = static_cast<std::byte>(tmp);
    return true;
  } else {
    return false;
  }
}

bool impl::value(bool& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(int8_t& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(uint8_t& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(int16_t& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(uint16_t& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(int32_t& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(uint32_t& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(int64_t& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(uint64_t& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(float& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(double& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(long double& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(std::string& x) {
  CHECK_NOT_EMPTY();
  return pull(*this, x);
}

bool impl::value(std::u16string&) {
  emplace_error(sec::runtime_error, "u16string support not implemented yet");
  return false;
}

bool impl::value(std::u32string&) {
  emplace_error(sec::runtime_error, "u32string support not implemented yet");
  return false;
}

bool impl::value(span<std::byte> bytes) {
  CHECK_NOT_EMPTY();
  std::string x;
  if (!pull(*this, x))
    return false;
  if (x.size() != bytes.size() * 2) {
    emplace_error(sec::runtime_error,
                  "hex-formatted string does not match expected size");
    return false;
  }
  for (size_t index = 0; index < x.size(); index += 2) {
    uint8_t value = 0;
    for (size_t i = 0; i < 2; ++i) {
      auto c = x[index + i];
      if (!isxdigit(c)) {
        emplace_error(sec::runtime_error,
                      "invalid character in hex-formatted string");
        return false;
      }
      detail::parser::add_ascii<16>(value, c);
    }
    bytes[index / 2] = static_cast<std::byte>(value);
  }
  return true;
}

bool impl::fetch_object_type(const settings* obj, type_id_t& type) {
  if (auto str = get_if<std::string>(obj, "@type"); str == nullptr) {
    // fetch_next_object_type only calls this function
    type = type_id_v<config_value::dictionary>;
    return true;
  } else if (auto id = query_type_id(*str); id != invalid_type_id) {
    type = id;
    return true;
  } else {
    emplace_error(sec::runtime_error, "unknown type: " + *str);
    return false;
  }
}

} // namespace

// -- constructors, destructors, and assignment operators ----------------------

config_value_reader::config_value_reader(const config_value* input) {
  impl::construct(impl_, input);
}

config_value_reader::config_value_reader(const config_value* input,
                                         actor_system& sys) {
  impl::construct(impl_, input, sys);
}

config_value_reader::~config_value_reader() {
  impl::destruct(impl_);
}

// -- interface functions ------------------------------------------------------

void config_value_reader::set_error(error stop_reason) {
  return impl::cast(impl_).set_error(std::move(stop_reason));
}

error& config_value_reader::get_error() noexcept {
  return impl::cast(impl_).get_error();
}

caf::actor_system* config_value_reader::sys() const noexcept {
  return impl::cast(impl_).sys();
}

bool config_value_reader::has_human_readable_format() const noexcept {
  return impl::cast(impl_).has_human_readable_format();
}

bool config_value_reader::fetch_next_object_type(type_id_t& type) {
  return impl::cast(impl_).fetch_next_object_type(type);
}

bool config_value_reader::begin_object(type_id_t type, std::string_view) {
  return impl::cast(impl_).begin_object(type, {});
}

bool config_value_reader::end_object() {
  return impl::cast(impl_).end_object();
}

bool config_value_reader::begin_field(std::string_view name) {
  return impl::cast(impl_).begin_field(name);
}

bool config_value_reader::begin_field(std::string_view name, bool& is_present) {
  return impl::cast(impl_).begin_field(name, is_present);
}

bool config_value_reader::begin_field(std::string_view name,
                                      span<const type_id_t> types,
                                      size_t& index) {
  return impl::cast(impl_).begin_field(name, types, index);
}

bool config_value_reader::begin_field(std::string_view name, bool& is_present,
                                      span<const type_id_t> types,
                                      size_t& index) {
  return impl::cast(impl_).begin_field(name, is_present, types, index);
}

bool config_value_reader::end_field() {
  return impl::cast(impl_).end_field();
}

bool config_value_reader::begin_tuple(size_t size) {
  return impl::cast(impl_).begin_tuple(size);
}

bool config_value_reader::end_tuple() {
  return impl::cast(impl_).end_tuple();
}

bool config_value_reader::begin_key_value_pair() {
  return impl::cast(impl_).begin_key_value_pair();
}

bool config_value_reader::end_key_value_pair() {
  return impl::cast(impl_).end_key_value_pair();
}

bool config_value_reader::begin_sequence(size_t& size) {
  return impl::cast(impl_).begin_sequence(size);
}

bool config_value_reader::end_sequence() {
  return impl::cast(impl_).end_sequence();
}

bool config_value_reader::begin_associative_array(size_t& size) {
  return impl::cast(impl_).begin_associative_array(size);
}

bool config_value_reader::end_associative_array() {
  return impl::cast(impl_).end_associative_array();
}

bool config_value_reader::value(std::byte& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(bool& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(int8_t& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(uint8_t& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(int16_t& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(uint16_t& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(int32_t& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(uint32_t& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(int64_t& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(uint64_t& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(float& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(double& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(long double& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(std::string& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(std::u16string& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(std::u32string& x) {
  return impl::cast(impl_).value(x);
}

bool config_value_reader::value(span<std::byte> bytes) {
  return impl::cast(impl_).value(bytes);
}

} // namespace caf
