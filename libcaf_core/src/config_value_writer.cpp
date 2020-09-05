/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config_value_writer.hpp"

#include "caf/config_value.hpp"
#include "caf/detail/append_hex.hpp"
#include "caf/detail/overload.hpp"
#include "caf/settings.hpp"

#define CHECK_NOT_EMPTY()                                                      \
  do {                                                                         \
    if (st_.empty()) {                                                         \
      emplace_error(sec::runtime_error, "mismatching calls to begin/end");     \
      return false;                                                            \
    }                                                                          \
  } while (false)

#define CHECK_VALID()                                                          \
  do {                                                                         \
    CHECK_NOT_EMPTY();                                                         \
    if (holds_alternative<none_t>(st_.top())) {                                \
      emplace_error(sec::runtime_error,                                        \
                    "attempted to write to a non-existent optional field");    \
      return false;                                                            \
    }                                                                          \
  } while (false)

#define SCOPE(top_type)                                                        \
  CHECK_VALID();                                                               \
  if (!holds_alternative<top_type>(st_.top())) {                               \
    if constexpr (std::is_same<top_type, settings>::value) {                   \
      emplace_error(sec::runtime_error,                                        \
                    "attempted to add list items before calling "              \
                    "begin_sequence or begin_tuple");                          \
    } else {                                                                   \
      emplace_error(sec::runtime_error,                                        \
                    "attempted to add fields to a list item");                 \
    }                                                                          \
    return false;                                                              \
  }                                                                            \
  [[maybe_unused]] auto& top = get<top_type>(st_.top());

namespace caf {

// -- constructors, destructors, and assignment operators ----------------------

config_value_writer::~config_value_writer() {
  // nop
}

// -- interface functions ------------------------------------------------------

bool config_value_writer::inject_next_object_type(type_id_t type) {
  CHECK_NOT_EMPTY();
  type_hint_ = query_type_name(type);
  if (type_hint_.empty()) {
    emplace_error(sec::runtime_error,
                  "query_type_name returned an empty string for type ID");
    return false;
  }
  return true;
}

bool config_value_writer::begin_object(string_view) {
  CHECK_NOT_EMPTY();
  auto f = detail::make_overload(
    [this](config_value* x) {
      // Morph the root element into a dictionary.
      auto& dict = x->as_dictionary();
      dict.clear();
      st_.top() = &dict;
      return true;
    },
    [this](settings*) {
      emplace_error(sec::runtime_error,
                    "begin_object called inside another object");
      return false;
    },
    [this](absent_field) {
      emplace_error(sec::runtime_error,
                    "begin_object called inside non-existent optional field");
      return false;
    },
    [this](present_field fld) {
      CAF_ASSERT(fld.parent != nullptr);
      auto [iter, added] = fld.parent->emplace(fld.name, settings{});
      if (!added) {
        emplace_error(sec::runtime_error,
                      "field already defined: " + to_string(fld.name));
        return false;
      }
      auto obj = std::addressof(get<settings>(iter->second));
      if (!fld.type.empty())
        put(*obj, "@type", fld.type);
      st_.push(obj);
      return true;
    },
    [this](config_value::list* ls) {
      ls->emplace_back(settings{});
      st_.push(std::addressof(get<settings>(ls->back())));
      return true;
    });
  if (!visit(f, st_.top()))
    return false;
  if (!type_hint_.empty()) {
    put(*get<settings*>(st_.top()), "@type", type_hint_);
    type_hint_ = string_view{};
  }
  return true;
}

bool config_value_writer::end_object() {
  SCOPE(settings*);
  st_.pop();
  return true;
}

bool config_value_writer::begin_field(string_view name) {
  SCOPE(settings*);
  st_.push(present_field{top, name, string_view{}});
  return true;
}

bool config_value_writer::begin_field(string_view name, bool is_present) {
  SCOPE(settings*);
  if (is_present)
    st_.push(present_field{top, name, string_view{}});
  else
    st_.push(absent_field{});
  return true;
}

bool config_value_writer::begin_field(string_view name,
                                      span<const type_id_t> types,
                                      size_t index) {
  SCOPE(settings*);
  if (index >= types.size()) {
    emplace_error(sec::invalid_argument,
                  "index out of range in optional variant field "
                    + to_string(name));
    return false;
  }
  auto tn = query_type_name(types[index]);
  if (tn.empty()) {
    emplace_error(sec::runtime_error,
                  "query_type_name returned an empty string for type ID");
    return false;
  }
  st_.push(present_field{top, name, tn});
  return true;
}

bool config_value_writer::begin_field(string_view name, bool is_present,
                                      span<const type_id_t> types,
                                      size_t index) {
  if (is_present)
    return begin_field(name, types, index);
  else
    return begin_field(name, false);
}

bool config_value_writer::end_field() {
  CHECK_NOT_EMPTY();
  if (!holds_alternative<present_field>(st_.top())
      && !holds_alternative<absent_field>(st_.top())) {
    emplace_error(sec::runtime_error, "end_field called outside of a field");
    return false;
  }
  st_.pop();
  return true;
}

bool config_value_writer::begin_tuple(size_t size) {
  return begin_sequence(size);
}

bool config_value_writer::end_tuple() {
  return end_sequence();
}

bool config_value_writer::begin_key_value_pair() {
  SCOPE(settings*);
  auto [iter, added] = top->emplace("@tmp", config_value::list{});
  if (!added) {
    emplace_error(sec::runtime_error, "temporary entry @tmp already exists");
    return false;
  }
  st_.push(std::addressof(get<config_value::list>(iter->second)));
  return true;
}

bool config_value_writer::end_key_value_pair() {
  config_value::list tmp;
  /* lifetime scope of the list */ {
    SCOPE(config_value::list*);
    if (top->size() != 2) {
      emplace_error(sec::runtime_error,
                    "a key-value pair must have exactly two elements");
      return false;
    }
    tmp = std::move(*top);
    st_.pop();
  }
  SCOPE(settings*);
  // Get key and value from the temporary list.
  top->container().erase("@tmp");
  std::string key;
  if (auto str = get_if<std::string>(std::addressof(tmp[0])))
    key = std::move(*str);
  else
    key = to_string(tmp[0]);
  if (!top->emplace(std::move(key), std::move(tmp[1])).second) {
    emplace_error(sec::runtime_error, "multiple definitions for key");
    return false;
  }
  return true;
}

bool config_value_writer::begin_sequence(size_t) {
  CHECK_NOT_EMPTY();
  auto f = detail::make_overload(
    [this](config_value* val) {
      // Morph the value into a list.
      auto& ls = val->as_list();
      ls.clear();
      st_.top() = &ls;
      return true;
    },
    [this](settings*) {
      emplace_error(sec::runtime_error,
                    "cannot start sequence/tuple inside an object");
      return false;
    },
    [this](absent_field) {
      emplace_error(
        sec::runtime_error,
        "cannot start sequence/tuple inside non-existent optional field");
      return false;
    },
    [this](present_field fld) {
      auto [iter, added] = fld.parent->emplace(fld.name, config_value::list{});
      if (!added) {
        emplace_error(sec::runtime_error,
                      "field already defined: " + to_string(fld.name));
        return false;
      }
      st_.push(std::addressof(get<config_value::list>(iter->second)));
      return true;
    },
    [this](config_value::list* ls) {
      ls->emplace_back(config_value::list{});
      st_.push(std::addressof(get<config_value::list>(ls->back())));
      return true;
    });
  return visit(f, st_.top());
}

bool config_value_writer::end_sequence() {
  SCOPE(config_value::list*);
  st_.pop();
  return true;
}

bool config_value_writer::begin_associative_array(size_t) {
  CHECK_NOT_EMPTY();
  settings* inner = nullptr;
  auto f = detail::make_overload(
    [this](config_value* val) {
      // Morph the top element into a dictionary.
      auto& dict = val->as_dictionary();
      dict.clear();
      st_.top() = &dict;
      return true;
    },
    [this](settings*) {
      emplace_error(sec::runtime_error, "cannot write values outside fields");
      return false;
    },
    [this](absent_field) {
      emplace_error(sec::runtime_error,
                    "cannot add values to non-existent optional field");
      return false;
    },
    [this, &inner](present_field fld) {
      auto [iter, added] = fld.parent->emplace(fld.name,
                                               config_value{settings{}});
      if (!added) {
        emplace_error(sec::runtime_error,
                      "field already defined: " + to_string(fld.name));
        return false;
      }
      if (!fld.type.empty()) {
        std::string key;
        key += '@';
        key.insert(key.end(), fld.name.begin(), fld.name.end());
        key += "-type";
        if (fld.parent->contains(key)) {
          emplace_error(sec::runtime_error,
                        "type of variant field already defined.");
          return false;
        }
        put(*fld.parent, key, fld.type);
      }
      inner = std::addressof(get<settings>(iter->second));
      return true;
    },
    [&inner](config_value::list* ls) {
      ls->emplace_back(config_value{settings{}});
      inner = std::addressof(get<settings>(ls->back()));
      return true;
    });
  if (visit(f, st_.top())) {
    CAF_ASSERT(inner != nullptr);
    st_.push(inner);
    return true;
  }
  return false;
}

bool config_value_writer::end_associative_array() {
  SCOPE(settings*);
  st_.pop();
  return true;
}

bool config_value_writer::value(bool x) {
  return push(config_value{x});
}

bool config_value_writer::value(int8_t x) {
  return push(config_value{static_cast<config_value::integer>(x)});
}

bool config_value_writer::value(uint8_t x) {
  return push(config_value{static_cast<config_value::integer>(x)});
}

bool config_value_writer::value(int16_t x) {
  return push(config_value{static_cast<config_value::integer>(x)});
}

bool config_value_writer::value(uint16_t x) {
  return push(config_value{static_cast<config_value::integer>(x)});
}

bool config_value_writer::value(int32_t x) {
  return push(config_value{static_cast<config_value::integer>(x)});
}

bool config_value_writer::value(uint32_t x) {
  return push(config_value{static_cast<config_value::integer>(x)});
}

bool config_value_writer::value(int64_t x) {
  return push(config_value{static_cast<config_value::integer>(x)});
}

bool config_value_writer::value(uint64_t x) {
  auto max_val = std::numeric_limits<config_value::integer>::max();
  if (x > static_cast<uint64_t>(max_val)) {
    emplace_error(sec::runtime_error, "integer overflow");
    return false;
  }
  return push(config_value{static_cast<config_value::integer>(x)});
}

bool config_value_writer::value(float x) {
  return push(config_value{double{x}});
}

bool config_value_writer::value(double x) {
  return push(config_value{x});
}

bool config_value_writer::value(long double x) {
  return push(config_value{std::to_string(x)});
}

bool config_value_writer::value(string_view x) {
  return push(config_value{to_string(x)});
}

bool config_value_writer::value(const std::u16string&) {
  emplace_error(sec::runtime_error, "u16string support not implemented yet");
  return false;
}

bool config_value_writer::value(const std::u32string&) {
  emplace_error(sec::runtime_error, "u32string support not implemented yet");
  return false;
}

bool config_value_writer::value(span<const byte> x) {
  std::string str;
  detail::append_hex(str, x.data(), x.size());
  return push(config_value{std::move(str)});
}

bool config_value_writer::push(config_value&& x) {
  CHECK_NOT_EMPTY();
  auto f = detail::make_overload(
    [&x](config_value* val) {
      *val = std::move(x);
      return true;
    },
    [this](settings*) {
      emplace_error(sec::runtime_error, "cannot write values outside fields");
      return false;
    },
    [this](absent_field) {
      emplace_error(sec::runtime_error,
                    "cannot add values to non-existent optional field");
      return false;
    },
    [this, &x](present_field fld) {
      auto [iter, added] = fld.parent->emplace(fld.name, std::move(x));
      if (!added) {
        emplace_error(sec::runtime_error,
                      "field already defined: " + to_string(fld.name));
        return false;
      }
      if (!fld.type.empty()) {
        std::string key;
        key += '@';
        key.insert(key.end(), fld.name.begin(), fld.name.end());
        key += "-type";
        if (fld.parent->contains(key)) {
          emplace_error(sec::runtime_error,
                        "type of variant field already defined.");
          return false;
        }
        put(*fld.parent, key, fld.type);
      }
      return true;
    },
    [&x](config_value::list* ls) {
      ls->emplace_back(std::move(x));
      return true;
    });
  return visit(f, st_.top());
}

} // namespace caf
