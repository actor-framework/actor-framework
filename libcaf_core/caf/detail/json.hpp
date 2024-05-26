// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/mbr_list.hpp"
#include "caf/detail/monotonic_buffer_resource.hpp"
#include "caf/detail/print.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/parser_state.hpp"
#include "caf/ref_counted.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <string_view>
#include <variant>

// This JSON abstraction is designed to allocate its entire state in a monotonic
// buffer resource. This minimizes memory allocations and also enables us to
// "wink out" the entire JSON object by simply reclaiming the memory without
// having to call a single destructor. The API is not optimized for convenience
// or safety, since the only place we use this API is the json_reader.

namespace caf::detail::json {

// -- utility classes ----------------------------------------------------------

// Wraps a buffer resource with a reference count.
struct CAF_CORE_EXPORT storage : public ref_counted {
  /// Provides the memory for all of our parsed JSON entities.
  detail::monotonic_buffer_resource buf;
};

// -- helper for modeling the JSON type system ---------------------------------

using storage_ptr = intrusive_ptr<storage>;

struct null_t {};

constexpr bool operator==(null_t, null_t) {
  return true;
}

constexpr bool operator!=(null_t, null_t) {
  return false;
}

struct undefined_t {};

constexpr bool operator==(undefined_t, undefined_t) {
  return true;
}

constexpr bool operator!=(undefined_t, undefined_t) {
  return false;
}

template <class T>
using linked_list_node = caf::detail::mbr_list_node<T>;

template <class T>
using linked_list_iterator = caf::detail::mbr_list_iterator<T>;

template <class T>
using linked_list = caf::detail::mbr_list<T>;

/// Re-allocates the given string at the buffer resource.
CAF_CORE_EXPORT std::string_view realloc(std::string_view str,
                                         monotonic_buffer_resource* res);

inline std::string_view realloc(std::string_view str, const storage_ptr& ptr) {
  return realloc(str, &ptr->buf);
}

/// Concatenates all strings and allocates a single new string for the result.
CAF_CORE_EXPORT std::string_view
concat(std::initializer_list<std::string_view> xs,
       monotonic_buffer_resource* res);

inline std::string_view concat(std::initializer_list<std::string_view> xs,
                               const storage_ptr& ptr) {
  return concat(xs, &ptr->buf);
}

class CAF_CORE_EXPORT value {
public:
  using array = linked_list<value>;

  using array_allocator = array::allocator_type;

  struct member {
    std::string_view key;
    value* val = nullptr;
  };

  using object = linked_list<member>;

  using object_allocator = object::allocator_type;

  using data_type = std::variant<null_t, int64_t, uint64_t, double, bool,
                                 std::string_view, array, object, undefined_t>;

  static constexpr size_t null_index = 0;

  static constexpr size_t integer_index = 1;

  static constexpr size_t unsigned_index = 2;

  static constexpr size_t double_index = 3;

  static constexpr size_t bool_index = 4;

  static constexpr size_t string_index = 5;

  static constexpr size_t array_index = 6;

  static constexpr size_t object_index = 7;

  static constexpr size_t undefined_index = 8;

  data_type data;

  bool is_null() const noexcept {
    return data.index() == null_index;
  }

  bool is_integer() const noexcept {
    return data.index() == integer_index;
  }

  bool is_unsigned() const noexcept {
    return data.index() == unsigned_index;
  }

  bool is_double() const noexcept {
    return data.index() == double_index;
  }

  bool is_bool() const noexcept {
    return data.index() == bool_index;
  }

  bool is_string() const noexcept {
    return data.index() == string_index;
  }

  bool is_array() const noexcept {
    return data.index() == array_index;
  }

  bool is_object() const noexcept {
    return data.index() == object_index;
  }

  bool is_undefined() const noexcept {
    return data.index() == undefined_index;
  }

  void assign_string(std::string_view str, monotonic_buffer_resource* res) {
    data = realloc(str, res);
  }

  void assign_string(std::string_view str, const storage_ptr& ptr) {
    data = realloc(str, &ptr->buf);
  }

  void assign_object(monotonic_buffer_resource* res) {
    data = object{object_allocator{res}};
  }

  void assign_object(const storage_ptr& ptr) {
    assign_object(&ptr->buf);
  }

  void assign_array(monotonic_buffer_resource* res) {
    data = array{array_allocator{res}};
  }

  void assign_array(const storage_ptr& ptr) {
    assign_array(&ptr->buf);
  }
};

inline bool operator==(const value& lhs, const value& rhs) {
  return lhs.data == rhs.data;
}

inline bool operator!=(const value& lhs, const value& rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const value::member& lhs, const value::member& rhs) {
  if (lhs.key == rhs.key && lhs.val != nullptr && rhs.val != nullptr) {
    return *lhs.val == *rhs.val;
  }
  return false;
}

inline bool operator!=(const value::member& lhs, const value::member& rhs) {
  return !(lhs == rhs);
}

using array = value::array;

using member = value::member;

using object = value::object;

// -- factory functions --------------------------------------------------------

CAF_CORE_EXPORT value* make_value(monotonic_buffer_resource* storage);

inline value* make_value(const storage_ptr& ptr) {
  return make_value(&ptr->buf);
}

CAF_CORE_EXPORT array* make_array(monotonic_buffer_resource* storage);

inline array* make_array(const storage_ptr& ptr) {
  return make_array(&ptr->buf);
}

CAF_CORE_EXPORT object* make_object(monotonic_buffer_resource* storage);

inline object* make_object(const storage_ptr& ptr) {
  return make_object(&ptr->buf);
}

// -- saving -------------------------------------------------------------------

template <class Serializer>
bool save(Serializer& sink, const object& obj);

template <class Serializer>
bool save(Serializer& sink, const array& arr);

template <class Serializer>
bool save(Serializer& sink, const value& val) {
  // On the "wire", we only use the public types.
  if (!sink.begin_object(type_id_v<json_value>, type_name_v<json_value>))
    return false;
  // Maps our type indexes to their public API counterpart.
  type_id_t mapping[]
    = {type_id_v<unit_t>,     type_id_v<int64_t>,     type_id_v<uint64_t>,
       type_id_v<double>,     type_id_v<bool>,        type_id_v<std::string>,
       type_id_v<json_array>, type_id_v<json_object>, type_id_v<none_t>};
  // Act as-if this type is a variant of the mapped types.
  auto type_index = val.data.index();
  if (!sink.begin_field("value", make_span(mapping), type_index))
    return false;
  // Dispatch on the run-time type of this value.
  switch (type_index) {
    case value::integer_index:
      if (!sink.apply(std::get<int64_t>(val.data)))
        return false;
      break;
    case value::unsigned_index:
      if (!sink.apply(std::get<uint64_t>(val.data)))
        return false;
      break;
    case value::double_index:
      if (!sink.apply(std::get<double>(val.data)))
        return false;
      break;
    case value::bool_index:
      if (!sink.apply(std::get<bool>(val.data)))
        return false;
      break;
    case value::string_index:
      if (!sink.apply(std::get<std::string_view>(val.data)))
        return false;
      break;
    case value::array_index:
      if (!save(sink, std::get<array>(val.data)))
        return false;
      break;
    case value::object_index:
      if (!save(sink, std::get<object>(val.data)))
        return false;
      break;
    default:
      // null and undefined both have no data
      break;
  }
  // Wrap up.
  return sink.end_field() && sink.end_object();
}

template <class Serializer>
bool save(Serializer& sink, const object& obj) {
  if (!sink.begin_associative_array(obj.size()))
    return false;
  for (const auto& kvp : obj) {
    if (kvp.val != nullptr) {
      if (!sink.begin_key_value_pair()   // <key-value-pair>
          || !sink.value(kvp.key)        //   <key ...>
          || !save(sink, *kvp.val)       //   <value ...>
          || !sink.end_key_value_pair()) // </key-value-pair>
        return false;
    }
  }
  return sink.end_associative_array();
}

template <class Serializer>
bool save(Serializer& sink, const array& arr) {
  if (!sink.begin_sequence(arr.size()))
    return false;
  for (const auto& val : arr)
    if (!save(sink, val))
      return false;
  return sink.end_sequence();
}

// -- loading ------------------------------------------------------------------

template <class Deserializer>
bool load(Deserializer& source, object& obj, monotonic_buffer_resource* res);

template <class Deserializer>
bool load(Deserializer& source, array& arr, monotonic_buffer_resource* res);

template <class Deserializer>
bool load(Deserializer& source, value& val, monotonic_buffer_resource* res) {
  // On the "wire", we only use the public types.
  if (!source.begin_object(type_id_v<json_value>, type_name_v<json_value>))
    return false;
  // Maps our type indexes to their public API counterpart.
  type_id_t mapping[]
    = {type_id_v<unit_t>,     type_id_v<int64_t>,     type_id_v<uint64_t>,
       type_id_v<double>,     type_id_v<bool>,        type_id_v<std::string>,
       type_id_v<json_array>, type_id_v<json_object>, type_id_v<none_t>};
  // Act as-if this type is a variant of the mapped types.
  auto type_index = size_t{0};
  if (!source.begin_field("value", make_span(mapping), type_index))
    return false;
  // Dispatch on the run-time type of this value.
  switch (type_index) {
    case value::null_index:
      val.data = null_t{};
      break;
    case value::integer_index: {
      auto tmp = int64_t{0};
      if (!source.apply(tmp))
        return false;
      val.data = tmp;
      break;
    }
    case value::unsigned_index: {
      auto tmp = uint64_t{0};
      if (!source.apply(tmp))
        return false;
      val.data = tmp;
      break;
    }
    case value::double_index: {
      auto tmp = double{0};
      if (!source.apply(tmp))
        return false;
      val.data = tmp;
      break;
    }
    case value::bool_index: {
      auto tmp = false;
      if (!source.apply(tmp))
        return false;
      val.data = tmp;
      break;
    }
    case value::string_index: {
      auto tmp = std::string{};
      if (!source.apply(tmp))
        return false;
      if (tmp.empty()) {
        val.data = std::string_view{};
      } else {
        val.assign_string(tmp, res);
      }
      break;
    }
    case value::array_index:
      val.data = array{array::allocator_type{res}};
      if (!load(source, std::get<array>(val.data), res))
        return false;
      break;
    case value::object_index:
      val.data = object{object::allocator_type{res}};
      if (!load(source, std::get<object>(val.data), res))
        return false;
      break;
    default: // undefined
      val.data = undefined_t{};
      break;
  }
  // Wrap up.
  return source.end_field() && source.end_object();
}

template <class Deserializer>
bool load(Deserializer& source, object& obj, monotonic_buffer_resource* res) {
  auto size = size_t{0};
  if (!source.begin_associative_array(size))
    return false;
  for (size_t i = 0; i < size; ++i) {
    if (!source.begin_key_value_pair())
      return false;
    auto& kvp = obj.emplace_back();
    // Deserialize the key.
    auto key = std::string{};
    if (!source.apply(key))
      return false;
    if (key.empty()) {
      kvp.key = std::string_view{};
    } else {
      kvp.key = realloc(key, res);
    }
    // Deserialize the value.
    kvp.val = make_value(res);
    if (!load(source, *kvp.val, res))
      return false;
    if (!source.end_key_value_pair())
      return false;
  }
  return source.end_associative_array();
}

template <class Deserializer>
bool load(Deserializer& source, array& arr, monotonic_buffer_resource* res) {
  auto size = size_t{0};
  if (!source.begin_sequence(size))
    return false;
  for (size_t i = 0; i < size; ++i) {
    auto& val = arr.emplace_back();
    if (!load(source, val, res))
      return false;
  }
  return source.end_sequence();
}

template <class Deserializer>
bool load(Deserializer& source, object& obj, const storage_ptr& ptr) {
  return load(source, obj, std::addressof(ptr->buf));
}

template <class Deserializer>
bool load(Deserializer& source, array& arr, const storage_ptr& ptr) {
  return load(source, arr, std::addressof(ptr->buf));
}

template <class Deserializer>
bool load(Deserializer& source, value& val, const storage_ptr& ptr) {
  return load(source, val, std::addressof(ptr->buf));
}

// -- parsing ------------------------------------------------------------------

// Specialization for parsers operating on mutable character sequences.
using mutable_string_parser_state = parser_state<char*>;

// Specialization for parsers operating on files.
using file_parser_state = parser_state<std::istreambuf_iterator<char>>;

// Parses the input string and makes a deep copy of all strings.
value* parse(string_parser_state& ps, monotonic_buffer_resource* storage);

// Parses the input string and makes a deep copy of all strings.
value* parse(file_parser_state& ps, monotonic_buffer_resource* storage);

// Parses the input file and makes a deep copy of all strings.
value* parse(string_parser_state& ps, monotonic_buffer_resource* storage);

// Parses the input and makes a shallow copy of strings whenever possible.
// Strings that do not have escaped characters are not copied, other strings
// will be copied.
value* parse_shallow(string_parser_state& ps,
                     monotonic_buffer_resource* storage);

// Parses the input and makes a shallow copy of all strings. Strings with
// escaped characters are decoded in place.
value* parse_in_situ(mutable_string_parser_state& ps,
                     monotonic_buffer_resource* storage);

// -- printing -----------------------------------------------------------------

template <class Buffer>
static void print_to(Buffer& buf, std::string_view str) {
  buf.insert(buf.end(), str.begin(), str.end());
}

template <class Buffer>
static void print_nl_to(Buffer& buf, size_t indentation) {
  buf.push_back('\n');
  buf.insert(buf.end(), indentation, ' ');
}

template <class Buffer>
void print_to(Buffer& buf, const array& arr, size_t indentation_factor,
              size_t offset = 0);

template <class Buffer>
void print_to(Buffer& buf, const object& obj, size_t indentation_factor,
              size_t offset = 0);

template <class Buffer>
void print_to(Buffer& buf, const value& val, size_t indentation_factor,
              size_t offset = 0) {
  using namespace std::literals;
  switch (val.data.index()) {
    case value::integer_index:
      print(buf, std::get<int64_t>(val.data));
      break;
    case value::unsigned_index:
      print(buf, std::get<uint64_t>(val.data));
      break;
    case value::double_index:
      print(buf, std::get<double>(val.data));
      break;
    case value::bool_index:
      print(buf, std::get<bool>(val.data));
      break;
    case value::string_index:
      print_escaped(buf, std::get<std::string_view>(val.data));
      break;
    case value::array_index:
      print_to(buf, std::get<array>(val.data), indentation_factor, offset);
      break;
    case value::object_index:
      print_to(buf, std::get<object>(val.data), indentation_factor, offset);
      break;
    default:
      print_to(buf, "null"sv);
  }
}
template <class Buffer>
void print_to(Buffer& buf, const array& arr, size_t indentation_factor,
              size_t offset) {
  using namespace std::literals;
  if (arr.empty()) {
    print_to(buf, "[]"sv);
  } else if (indentation_factor == 0) {
    buf.push_back('[');
    auto i = arr.begin();
    print_to(buf, *i, 0);
    auto e = arr.end();
    while (++i != e) {
      print_to(buf, ", "sv);
      print_to(buf, *i, 0);
    }
    buf.push_back(']');
  } else {
    buf.push_back('[');
    auto new_offset = indentation_factor + offset;
    print_nl_to(buf, new_offset);
    auto i = arr.begin();
    print_to(buf, *i, indentation_factor, new_offset);
    auto e = arr.end();
    while (++i != e) {
      buf.push_back(',');
      print_nl_to(buf, new_offset);
      print_to(buf, *i, indentation_factor, new_offset);
    }
    print_nl_to(buf, offset);
    buf.push_back(']');
  }
}

template <class Buffer>
void print_to(Buffer& buf, const object& obj, size_t indentation_factor,
              size_t offset) {
  using namespace std::literals;
  auto print_member = [&](const value::member& kvp, size_t new_offset) {
    print_escaped(buf, kvp.key);
    print_to(buf, ": "sv);
    print_to(buf, *kvp.val, indentation_factor, new_offset);
  };
  if (obj.empty()) {
    print_to(buf, "{}"sv);
  } else if (indentation_factor == 0) {
    buf.push_back('{');
    auto i = obj.begin();
    print_member(*i, offset);
    auto e = obj.end();
    while (++i != e) {
      print_to(buf, ", "sv);
      print_member(*i, offset);
    }
    buf.push_back('}');
  } else {
    buf.push_back('{');
    auto new_offset = indentation_factor + offset;
    print_nl_to(buf, new_offset);
    auto i = obj.begin();
    print_member(*i, new_offset);
    auto e = obj.end();
    while (++i != e) {
      buf.push_back(',');
      print_nl_to(buf, new_offset);
      print_member(*i, new_offset);
    }
    print_nl_to(buf, offset);
    buf.push_back('}');
  }
}

} // namespace caf::detail::json
