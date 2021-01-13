// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/json.hpp"

#include <iterator>
#include <memory>

#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/read_bool.hpp"
#include "caf/detail/parser/read_number.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

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
  string_view* ptr;

  void value(string_view str) {
    *ptr = str;
  }
};

struct member_consumer {
  monotonic_buffer_resource* storage;
  json::member* ptr;

  key_consumer begin_key() {
    return {std::addressof(ptr->key)};
  }

  val_consumer begin_val() {
    ptr->val = json::make_value(storage);
    return {storage, ptr->val};
  }
};

struct obj_consumer {
  json::object* ptr;

  member_consumer begin_member() {
    ptr->emplace_back();
    return {ptr->get_allocator().resource(), std::addressof(ptr->back())};
  }
};

struct arr_consumer {
  json::array* ptr;

  val_consumer begin_value() {
    ptr->emplace_back();
    return {ptr->get_allocator().resource(), std::addressof(ptr->back())};
  }
};

arr_consumer val_consumer::begin_array() {
  ptr->data = json::array(json::value::array_allocator{storage});
  auto& arr = std::get<json::array>(ptr->data);
  arr.reserve(16);
  return {&arr};
}

obj_consumer val_consumer::begin_object() {
  ptr->data = json::object(json::value::member_allocator{storage});
  auto& obj = std::get<json::object>(ptr->data);
  obj.reserve(16);
  return {&obj};
}

void read_value(string_parser_state& ps, val_consumer consumer);

void read_json_null(string_parser_state& ps, val_consumer consumer) {
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(json::null_t{});
  });
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(has_n, 'n')
  }
  state(has_n) {
    transition(has_nu, 'u')
  }
  state(has_nu) {
    transition(has_nul, 'l')
  }
  state(has_nul) {
    transition(done, 'l')
  }
  term_state(done) {
    transition(init, " \t\n")
  }
  fin();
  // clang-format on
}

template <class Consumer>
void read_json_string(string_parser_state& ps, Consumer consumer) {
  auto first = string_view::iterator{};
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(read_chars, '"', first = ps.i + 1)
  }
  state(read_chars) {
    transition(escape, '\\')
    transition(done, '"', consumer.value(string_view{first, ps.i}))
    transition(read_chars, any_char)
  }
  state(escape) {
    // TODO: Add support for JSON's \uXXXX escaping.
    transition(read_chars, "\"\\/bfnrt")
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

void read_member(string_parser_state& ps, member_consumer consumer) {
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    fsm_epsilon(read_json_string(ps, consumer.begin_key()), after_key, '"')
  }
  state(after_key) {
    transition(after_key, " \t\n")
    fsm_transition(read_value(ps, consumer.begin_val()), done, ':')
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

void read_json_object(string_parser_state& ps, obj_consumer consumer) {
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(has_open_brace, '{')
  }
  state(has_open_brace) {
    transition(has_open_brace, " \t\n")
    fsm_epsilon(read_member(ps, consumer.begin_member()), after_member, '"')
    transition(done, '}')
  }
  state(after_member) {
    transition(after_member, " \t\n")
    transition(after_comma, ',')
    transition(done, '}')
  }
  state(after_comma) {
    transition(after_comma, " \t\n")
    fsm_epsilon(read_member(ps, consumer.begin_member()), after_member, '"')
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

void read_json_array(string_parser_state& ps, arr_consumer consumer) {
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(has_open_brace, '[')
  }
  state(has_open_brace) {
    transition(has_open_brace, " \t\n")
    transition(done, ']')
    fsm_epsilon(read_value(ps, consumer.begin_value()), after_value)
  }
  state(after_value) {
    transition(after_value, " \t\n")
    transition(after_comma, ',')
    transition(done, ']')
  }
  state(after_comma) {
    transition(after_comma, " \t\n")
    fsm_epsilon(read_value(ps, consumer.begin_value()), after_value)
  }
  term_state(done) {
    transition(done, " \t\n")
  }
  fin();
  // clang-format on
}

void read_value(string_parser_state& ps, val_consumer consumer) {
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    fsm_epsilon(read_json_string(ps, consumer), done, '"')
    fsm_epsilon(read_bool(ps, consumer), done, "ft")
    fsm_epsilon(read_json_null(ps, consumer), done, "n")
    fsm_epsilon(read_number(ps, consumer), done, "+-.0123456789")
    fsm_epsilon(read_json_object(ps, consumer.begin_object()), done, '{')
    fsm_epsilon(read_json_array(ps, consumer.begin_array()), done, '[')
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

template <class T, class Allocator>
void init(std::vector<T, Allocator>* ptr, monotonic_buffer_resource* storage) {
  new (ptr) std::vector<T, Allocator>(Allocator{storage});
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

} // namespace

value* make_value(monotonic_buffer_resource* storage) {
  return make_impl<value>(storage);
}

array* make_array(monotonic_buffer_resource* storage) {
  auto result = make_impl<array>(storage);
  result->reserve(16);
  return result;
}

object* make_object(monotonic_buffer_resource* storage) {
  auto result = make_impl<object>(storage);
  result->reserve(16);
  return result;
}

value* parse(string_parser_state& ps, monotonic_buffer_resource* storage) {
  monotonic_buffer_resource::allocator<value> alloc{storage};
  auto result = new (alloc.allocate(1)) value();
  parser::read_value(ps, {storage, result});
  return result;
}

} // namespace caf::detail::json
