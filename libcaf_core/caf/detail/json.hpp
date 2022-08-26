// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/monotonic_buffer_resource.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/parser_state.hpp"
#include "caf/ref_counted.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <new>
#include <string_view>
#include <variant>

// This JSON abstraction is designed to allocate its entire state in a monotonic
// buffer resource. This minimizes memory allocations and also enables us to
// "wink out" the entire JSON object by simply reclaiming the memory without
// having to call a single destructor. The API is not optimized for convenience
// or safety, since the only place we use this API is the json_reader.

namespace caf::detail::json {

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
struct linked_list_node {
  T value;
  linked_list_node* next;
};

template <class T>
class linked_list_iterator {
public:
  using difference_type = ptrdiff_t;

  using value_type = T;

  using pointer = value_type*;

  using reference = value_type&;

  using iterator_category = std::forward_iterator_tag;

  using node_pointer
    = std::conditional_t<std::is_const_v<T>,
                         const linked_list_node<std::remove_const_t<T>>*,
                         linked_list_node<value_type>*>;

  constexpr linked_list_iterator() noexcept = default;

  constexpr explicit linked_list_iterator(node_pointer ptr) noexcept
    : ptr_(ptr) {
    // nop
  }

  constexpr linked_list_iterator(
    const linked_list_iterator&) noexcept = default;

  constexpr linked_list_iterator&
  operator=(const linked_list_iterator&) noexcept = default;

  constexpr node_pointer get() const noexcept {
    return ptr_;
  }

  linked_list_iterator& operator++() noexcept {
    ptr_ = ptr_->next;
    return *this;
  }

  linked_list_iterator operator++(int) noexcept {
    return linked_list_iterator{ptr_->next};
  }

  T& operator*() const noexcept {
    return ptr_->value;
  }

  T* operator->() const noexcept {
    return std::addressof(ptr_->value);
  }

private:
  node_pointer ptr_ = nullptr;
};

template <class T>
constexpr bool
operator==(linked_list_iterator<T> lhs, linked_list_iterator<T> rhs) {
  return lhs.get() == rhs.get();
}

template <class T>
constexpr bool
operator!=(linked_list_iterator<T> lhs, linked_list_iterator<T> rhs) {
  return !(lhs == rhs);
}

// A minimal version of a linked list that has constexpr constructor and an
// iterator type where the default-constructed iterator is the past-the-end
// iterator. Properties that std::list unfortunately lacks.
//
// The default-constructed list object is an empty list that does not allow
// push_back.
template <class T>
class linked_list {
public:
  using value_type = T;

  using node_type = linked_list_node<value_type>;

  using allocator_type = monotonic_buffer_resource::allocator<node_type>;

  using reference = value_type&;

  using const_reference = const value_type&;

  using pointer = value_type*;

  using const_pointer = const value_type*;

  using node_pointer = node_type*;

  using iterator = linked_list_iterator<value_type>;

  using const_iterator = linked_list_iterator<const value_type>;

  linked_list() noexcept {
    // nop
  }

  ~linked_list() {
    auto* ptr = head_;
    while (ptr != nullptr) {
      auto* next = ptr->next;
      ptr->~node_type();
      allocator_.deallocate(ptr, 1);
      ptr = next;
    }
  }

  explicit linked_list(allocator_type allocator) noexcept
    : allocator_(allocator) {
    // nop
  }

  linked_list(const linked_list&) = delete;

  linked_list(linked_list&& other)
    : size_(other.size_),
      head_(other.head_),
      tail_(other.tail_),
      allocator_(other.allocator_) {
    other.size_ = 0;
    other.head_ = nullptr;
    other.tail_ = nullptr;
  }

  linked_list& operator=(const linked_list&) = delete;

  linked_list& operator=(linked_list&& other) {
    using std::swap;
    swap(size_, other.size_);
    swap(head_, other.head_);
    swap(tail_, other.tail_);
    swap(allocator_, other.allocator_);
    return *this;
  }

  [[nodiscard]] bool empty() const noexcept {
    return size_ == 0;
  }

  [[nodiscard]] size_t size() const noexcept {
    return size_;
  }

  [[nodiscard]] iterator begin() noexcept {
    return iterator{head_};
  }

  [[nodiscard]] const_iterator begin() const noexcept {
    return const_iterator{head_};
  }

  [[nodiscard]] const_iterator cbegin() const noexcept {
    return begin();
  }

  [[nodiscard]] iterator end() noexcept {
    return {};
  }

  [[nodiscard]] const_iterator end() const noexcept {
    return {};
  }

  [[nodiscard]] const_iterator cend() const noexcept {
    return {};
  }

  [[nodiscard]] reference front() noexcept {
    return head_->value;
  }

  [[nodiscard]] const_reference front() const noexcept {
    return head_->value;
  }

  [[nodiscard]] reference back() noexcept {
    return tail_->value;
  }

  [[nodiscard]] const_reference back() const noexcept {
    return tail_->value;
  }

  [[nodiscard]] allocator_type get_allocator() const noexcept {
    return allocator_;
  }

  void push_back(T value) {
    ++size_;
    auto new_node = allocator_->allocate(1);
    new (new_node) node_type{std::move(value), nullptr};
    if (head_ == nullptr) {
      head_ = tail_ = new_node;
    } else {
      tail_->next = new_node;
      tail_ = new_node;
    }
  }

  template <class... Ts>
  reference emplace_back(Ts&&... args) {
    ++size_;
    auto new_node = allocator_.allocate(1);
    new (new_node) node_type{T{std::forward<Ts>(args)...}, nullptr};
    if (head_ == nullptr) {
      head_ = tail_ = new_node;
    } else {
      tail_->next = new_node;
      tail_ = new_node;
    }
    return new_node->value;
  }

private:
  size_t size_ = 0;
  node_pointer head_ = nullptr;
  node_pointer tail_ = nullptr;
  allocator_type allocator_;
};

template <class T>
bool operator==(const linked_list<T>& lhs, const linked_list<T>& rhs) {
  return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <class T>
bool operator!=(const linked_list<T>& lhs, const linked_list<T>& rhs) {
  return !(lhs == rhs);
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

  using data_type = std::variant<null_t, int64_t, double, bool,
                                 std::string_view, array, object, undefined_t>;

  static constexpr size_t null_index = 0;

  static constexpr size_t integer_index = 1;

  static constexpr size_t double_index = 2;

  static constexpr size_t bool_index = 3;

  static constexpr size_t string_index = 4;

  static constexpr size_t array_index = 5;

  static constexpr size_t object_index = 6;

  static constexpr size_t undefined_index = 7;

  data_type data;

  bool is_null() const noexcept {
    return data.index() == null_index;
  }

  bool is_integer() const noexcept {
    return data.index() == integer_index;
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

// -- utility classes ----------------------------------------------------------

// Wraps a buffer resource with a reference count.
struct CAF_CORE_EXPORT storage : public ref_counted {
  /// Provides the memory for all of our parsed JSON entities.
  detail::monotonic_buffer_resource buf;
};

using storage_ptr = intrusive_ptr<storage>;

// -- factory functions --------------------------------------------------------

value* make_value(monotonic_buffer_resource* storage);

inline value* make_value(const storage_ptr& ptr) {
  return make_value(&ptr->buf);
}

array* make_array(monotonic_buffer_resource* storage);

inline array* make_array(const storage_ptr& ptr) {
  return make_array(&ptr->buf);
}

object* make_object(monotonic_buffer_resource* storage);

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
  type_id_t mapping[] = {type_id_v<unit_t>,      type_id_v<int64_t>,
                         type_id_v<double>,      type_id_v<bool>,
                         type_id_v<std::string>, type_id_v<json_array>,
                         type_id_v<json_object>, type_id_v<none_t>};
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
  type_id_t mapping[] = {type_id_v<unit_t>,      type_id_v<int64_t>,
                         type_id_v<double>,      type_id_v<bool>,
                         type_id_v<std::string>, type_id_v<json_array>,
                         type_id_v<json_object>, type_id_v<none_t>};
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
        using alloc_t = detail::monotonic_buffer_resource::allocator<char>;
        auto buf = alloc_t{res}.allocate(tmp.size());
        strncpy(buf, tmp.data(), tmp.size());
        val.data = std::string_view{buf, tmp.size()};
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
      using alloc_t = detail::monotonic_buffer_resource::allocator<char>;
      auto buf = alloc_t{res}.allocate(key.size());
      strncpy(buf, key.data(), key.size());
      kvp.key = std::string_view{buf, key.size()};
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

// -- singletons ---------------------------------------------------------------

const value* null_value() noexcept;

const value* undefined_value() noexcept;

const object* empty_object() noexcept;

const array* empty_array() noexcept;

// -- parsing ------------------------------------------------------------------

// Parses the input and makes a deep copy of all strings.
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

} // namespace caf::detail::json
