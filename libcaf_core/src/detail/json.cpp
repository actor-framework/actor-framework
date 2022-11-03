// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/json.hpp"

#include <cstring>
#include <iterator>
#include <memory>
#include <numeric>
#include <streambuf>

#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/read_bool.hpp"
#include "caf/detail/parser/read_number.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"
#include "caf/span.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace {

constexpr size_t max_nesting_level = 128;

size_t do_unescape(const char* i, const char* e, char* out) {
  size_t new_size = 0;
  while (i != e) {
    switch (*i) {
      default:
        if (i != out) {
          *out++ = *i++;
        } else {
          ++out;
          ++i;
        }
        ++new_size;
        break;
      case '\\':
        if (++i != e) {
          switch (*i) {
            case '"':
              *out++ = '"';
              break;
            case '\\':
              *out++ = '\\';
              break;
            case 'b':
              *out++ = '\b';
              break;
            case 'f':
              *out++ = '\f';
              break;
            case 'n':
              *out++ = '\n';
              break;
            case 'r':
              *out++ = '\r';
              break;
            case 't':
              *out++ = '\t';
              break;
            case 'v':
              *out++ = '\v';
              break;
            default:
              // TODO: support control characters in \uXXXX notation.
              *out++ = '?';
          }
          ++i;
          ++new_size;
        }
    }
  }
  return new_size;
}

std::string_view as_str_view(const char* first, const char* last) {
  return {first, static_cast<size_t>(last - first)};
}

struct regular_unescaper {
  std::string_view operator()(caf::detail::monotonic_buffer_resource* storage,
                              const char* first, const char* last,
                              bool is_escaped) const {
    auto len = static_cast<size_t>(last - first);
    caf::detail::monotonic_buffer_resource::allocator<char> alloc{storage};
    auto* str_buf = alloc.allocate(len);
    if (!is_escaped) {
      strncpy(str_buf, first, len);
      return std::string_view{str_buf, len};
    }
    auto unescaped_size = do_unescape(first, last, str_buf);
    return std::string_view{str_buf, unescaped_size};
  }
};

struct shallow_unescaper {
  std::string_view operator()(caf::detail::monotonic_buffer_resource* storage,
                              const char* first, const char* last,
                              bool is_escaped) const {
    if (!is_escaped)
      return as_str_view(first, last);
    caf::detail::monotonic_buffer_resource::allocator<char> alloc{storage};
    auto* str_buf = alloc.allocate(static_cast<size_t>(last - first));
    auto unescaped_size = do_unescape(first, last, str_buf);
    return std::string_view{str_buf, unescaped_size};
  }
};

struct in_situ_unescaper {
  std::string_view operator()(caf::detail::monotonic_buffer_resource*,
                              char* first, char* last, bool is_escaped) const {
    if (!is_escaped)
      return as_str_view(first, last);
    auto unescaped_size = do_unescape(first, last, first);
    return std::string_view{first, unescaped_size};
  }
};

template <class Escaper, class Consumer, class Iterator>
void assign_value(Escaper escaper, Consumer& consumer, Iterator first,
                  Iterator last, bool is_escaped) {
  auto iter2ptr = [](Iterator iter) {
    if constexpr (std::is_pointer_v<Iterator>)
      return iter;
    else
      return std::addressof(*iter);
  };
  auto str = escaper(consumer.storage, iter2ptr(first), iter2ptr(last),
                     is_escaped);
  consumer.value(str);
}

} // namespace

namespace caf::detail::parser {

struct obj_consumer;

struct arr_consumer;

struct val_consumer {
  monotonic_buffer_resource* storage;
  json::value* ptr;

  template <class T>
  void value(T x) {
    ptr->data = x;
  }

  arr_consumer begin_array();

  obj_consumer begin_object();
};

struct key_consumer {
  monotonic_buffer_resource* storage;
  std::string_view* ptr;

  void value(std::string_view str) {
    *ptr = str;
  }
};

struct member_consumer {
  monotonic_buffer_resource* storage;
  json::member* ptr;

  key_consumer begin_key() {
    return {storage, std::addressof(ptr->key)};
  }

  val_consumer begin_val() {
    ptr->val = json::make_value(storage);
    return {storage, ptr->val};
  }
};

struct obj_consumer {
  json::object* ptr;

  member_consumer begin_member() {
    auto& new_member = ptr->emplace_back();
    return {ptr->get_allocator().resource(), &new_member};
  }
};

struct arr_consumer {
  json::array* ptr;

  val_consumer begin_value() {
    auto& new_element = ptr->emplace_back();
    return {ptr->get_allocator().resource(), &new_element};
  }
};

arr_consumer val_consumer::begin_array() {
  ptr->data = json::array(json::value::array_allocator{storage});
  auto& arr = std::get<json::array>(ptr->data);
  return {&arr};
}

obj_consumer val_consumer::begin_object() {
  ptr->data = json::object(json::value::object_allocator{storage});
  auto& obj = std::get<json::object>(ptr->data);
  return {&obj};
}

template <class ParserState, class Consumer>
void read_json_null_or_nan(ParserState& ps, Consumer consumer) {
  enum { nil, is_null, is_nan };
  auto res_type = nil;
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      CAF_ASSERT(res_type != nil);
      if (res_type == is_null)
        consumer.value(json::null_t{});
      else
        consumer.value(std::numeric_limits<double>::quiet_NaN());
    }
  });
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(has_n, 'n')
  }
  state(has_n) {
    transition(has_nu, 'u')
    transition(has_na, 'a')
  }
  state(has_nu) {
    transition(has_nul, 'l')
  }
  state(has_nul) {
    transition(done, 'l', res_type = is_null)
  }
  state(has_na) {
    transition(done, 'n', res_type = is_nan)
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

// If we have an iterator into a contiguous memory block, we simply store the
// iterator position and use the escaper to decide whether we make regular,
// shallow or in-situ copies. Otherwise, we use the scratch-space and decode the
// string while parsing.

template <class ParserState, class Unescaper, class Consumer>
void read_json_string(ParserState& ps, unit_t, Unescaper escaper,
                      Consumer consumer) {
  using iterator_t = typename ParserState::iterator_type;
  iterator_t first;
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(read_chars, '"', first = ps.i + 1)
  }
  state(read_chars) {
    transition(escape, '\\')
    transition(done, '"', assign_value(escaper, consumer, first, ps.i, false))
    transition(read_chars, any_char)
  }
  state(read_chars_after_escape) {
    transition(escape, '\\')
    transition(done, '"', assign_value(escaper, consumer, first, ps.i, true))
    transition(read_chars_after_escape, any_char)
  }
  state(escape) {
    // TODO: Add support for JSON's \uXXXX escaping.
    transition(read_chars_after_escape, "\"\\/bfnrtv")
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

template <class ParserState, class Unescaper, class Consumer>
void read_json_string(ParserState& ps, std::vector<char>& scratch_space,
                      Unescaper escaper, Consumer consumer) {
  scratch_space.clear();
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(read_chars, '"')
  }
  state(read_chars) {
    transition(escape, '\\')
    transition(done, '"',
               assign_value(escaper, consumer, scratch_space.begin(),
                            scratch_space.end(), false))
    transition(read_chars, any_char, scratch_space.push_back(ch))
  }
  state(escape) {
    // TODO: Add support for JSON's \uXXXX escaping.
    transition(read_chars, '"', scratch_space.push_back('"'))
    transition(read_chars, '\\', scratch_space.push_back('\\'))
    transition(read_chars, 'b', scratch_space.push_back('\b'))
    transition(read_chars, 'f', scratch_space.push_back('\f'))
    transition(read_chars, 'n', scratch_space.push_back('\n'))
    transition(read_chars, 'r', scratch_space.push_back('\r'))
    transition(read_chars, 't', scratch_space.push_back('\t'))
    transition(read_chars, 'v', scratch_space.push_back('\v'))
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

template <class ParserState, class ScratchSpace, class Unescaper>
void read_member(ParserState& ps, ScratchSpace& scratch_space,
                 Unescaper unescaper, size_t nesting_level,
                 member_consumer consumer) {
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    fsm_epsilon(read_json_string(ps, scratch_space, unescaper,
                                 consumer.begin_key()),
                after_key, '"')
  }
  state(after_key) {
    transition(after_key, " \t\n")
    fsm_transition(read_value(ps, scratch_space, unescaper, nesting_level,
                              consumer.begin_val()),
                   done, ':')
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

template <class ParserState, class ScratchSpace, class Unescaper>
void read_json_object(ParserState& ps, ScratchSpace& scratch_space,
                      Unescaper unescaper, size_t nesting_level,
                      obj_consumer consumer) {
  if (nesting_level >= max_nesting_level) {
    ps.code = pec::nested_too_deeply;
    return;
  }
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(has_open_brace, '{')
  }
  state(has_open_brace) {
    transition(has_open_brace, " \t\n")
    fsm_epsilon(read_member(ps, scratch_space, unescaper, nesting_level + 1,
                            consumer.begin_member()),
                after_member, '"')
    transition(done, '}')
  }
  state(after_member) {
    transition(after_member, " \t\n")
    transition(after_comma, ',')
    transition(done, '}')
  }
  state(after_comma) {
    transition(after_comma, " \t\n")
    fsm_epsilon(read_member(ps, scratch_space, unescaper, nesting_level + 1,
                            consumer.begin_member()),
                after_member, '"')
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

template <class ParserState, class ScratchSpace, class Unescaper>
void read_json_array(ParserState& ps, ScratchSpace& scratch_space,
                     Unescaper unescaper, size_t nesting_level,
                     arr_consumer consumer) {
  if (nesting_level >= max_nesting_level) {
    ps.code = pec::nested_too_deeply;
    return;
  }
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(has_open_brace, '[')
  }
  state(has_open_brace) {
    transition(has_open_brace, " \t\n")
    transition(done, ']')
    fsm_epsilon(read_value(ps, scratch_space, unescaper, nesting_level + 1,
                           consumer.begin_value()),
                after_value)
  }
  state(after_value) {
    transition(after_value, " \t\n")
    transition(after_comma, ',')
    transition(done, ']')
  }
  state(after_comma) {
    transition(after_comma, " \t\n")
    fsm_epsilon(read_value(ps, scratch_space, unescaper, nesting_level + 1,
                           consumer.begin_value()),
                after_value)
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

template <class ParserState, class ScratchSpace, class Unescaper>
void read_value(ParserState& ps, ScratchSpace& scratch_space,
                Unescaper unescaper, size_t nesting_level,
                val_consumer consumer) {
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    fsm_epsilon(read_json_string(ps, scratch_space, unescaper, consumer),
                done, '"')
    fsm_epsilon(read_bool(ps, consumer), done, "ft")
    fsm_epsilon(read_json_null_or_nan(ps, consumer), done, "n")
    fsm_epsilon(read_number(ps, consumer), done, "+-.0123456789")
    fsm_epsilon(read_json_object(ps, scratch_space, unescaper, nesting_level,
                                 consumer.begin_object()),
                done, '{')
    fsm_epsilon(read_json_array(ps, scratch_space, unescaper, nesting_level,
                                consumer.begin_array()),
                done, '[')
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

namespace caf::detail::json {

namespace {

template <class T>
void init(linked_list<T>* ptr, monotonic_buffer_resource* storage) {
  using allocator_type = typename linked_list<T>::allocator_type;
  new (ptr) linked_list<T>(allocator_type{storage});
}

void init(value* ptr, monotonic_buffer_resource*) {
  new (ptr) value();
}

template <class T>
T* make_impl(monotonic_buffer_resource* storage) {
  monotonic_buffer_resource::allocator<T> alloc{storage};
  auto result = alloc.allocate(1);
  init(result, storage);
  return result;
}

const value null_value_instance;

const value undefined_value_instance = value{undefined_t{}};

const object empty_object_instance;

const array empty_array_instance;

} // namespace

std::string_view realloc(std::string_view str, monotonic_buffer_resource* res) {
  using alloc_t = detail::monotonic_buffer_resource::allocator<char>;
  auto buf = alloc_t{res}.allocate(str.size());
  strncpy(buf, str.data(), str.size());
  return std::string_view{buf, str.size()};
}

std::string_view concat(std::initializer_list<std::string_view> xs,
                        monotonic_buffer_resource* res) {
  auto get_size = [](size_t x, std::string_view str) { return x + str.size(); };
  auto total_size = std::accumulate(xs.begin(), xs.end(), size_t{0}, get_size);
  using alloc_t = detail::monotonic_buffer_resource::allocator<char>;
  auto* buf = alloc_t{res}.allocate(total_size);
  auto* pos = buf;
  for (auto str : xs) {
    strncpy(pos, str.data(), str.size());
    pos += str.size();
  }
  return std::string_view{buf, total_size};
}

value* make_value(monotonic_buffer_resource* storage) {
  return make_impl<value>(storage);
}

array* make_array(monotonic_buffer_resource* storage) {
  auto result = make_impl<array>(storage);
  return result;
}

object* make_object(monotonic_buffer_resource* storage) {
  auto result = make_impl<object>(storage);
  return result;
}

const value* null_value() noexcept {
  return &null_value_instance;
}

const value* undefined_value() noexcept {
  return &undefined_value_instance;
}

const object* empty_object() noexcept {
  return &empty_object_instance;
}

const array* empty_array() noexcept {
  return &empty_array_instance;
}

value* parse(string_parser_state& ps, monotonic_buffer_resource* storage) {
  unit_t scratch_space;
  regular_unescaper unescaper;
  monotonic_buffer_resource::allocator<value> alloc{storage};
  auto result = new (alloc.allocate(1)) value();
  parser::read_value(ps, scratch_space, unescaper, 0, {storage, result});
  return result;
}

value* parse(file_parser_state& ps, monotonic_buffer_resource* storage) {
  std::vector<char> scratch_space;
  scratch_space.reserve(64);
  regular_unescaper unescaper;
  monotonic_buffer_resource::allocator<value> alloc{storage};
  auto result = new (alloc.allocate(1)) value();
  parser::read_value(ps, scratch_space, unescaper, 0, {storage, result});
  return result;
}

value* parse_shallow(string_parser_state& ps,
                     monotonic_buffer_resource* storage) {
  unit_t scratch_space;
  shallow_unescaper unescaper;
  monotonic_buffer_resource::allocator<value> alloc{storage};
  auto result = new (alloc.allocate(1)) value();
  parser::read_value(ps, scratch_space, unescaper, 0, {storage, result});
  return result;
}

value* parse_in_situ(mutable_string_parser_state& ps,
                     monotonic_buffer_resource* storage) {
  unit_t scratch_space;
  in_situ_unescaper unescaper;
  monotonic_buffer_resource::allocator<value> alloc{storage};
  auto result = new (alloc.allocate(1)) value();
  parser::read_value(ps, scratch_space, unescaper, 0, {storage, result});
  return result;
}

} // namespace caf::detail::json
