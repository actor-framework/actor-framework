/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iterator>
#include <exception>
#include <stdexcept>
#include <type_traits>

#include "caf/binary_deserializer.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/uniform_type_info_map.hpp"

namespace caf {

namespace {

using pointer = const void*;

const char* as_char_pointer(pointer ptr) {
  return reinterpret_cast<const char*>(ptr);
}

pointer advanced(pointer ptr, size_t num_bytes) {
  return reinterpret_cast<const char*>(ptr) + num_bytes;
}

inline void range_check(pointer begin, pointer end, size_t read_size) {
  if (advanced(begin, read_size) > end) {
    CAF_LOGF(CAF_ERROR, "range_check failed");
    throw std::out_of_range("binary_deserializer::read_range()");
  }
}

pointer read_range(pointer begin, pointer end, std::string& storage);

template <class T>
typename std::enable_if<std::is_integral<T>::value, pointer>::type
read_range(pointer begin, pointer end, T& storage) {
  range_check(begin, end, sizeof(T));
  memcpy(&storage, begin, sizeof(T));
  return advanced(begin, sizeof(T));
}

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, pointer>::type
read_range(pointer begin, pointer end, T& storage) {
  typename detail::ieee_754_trait<T>::packed_type tmp;
  auto result = read_range(begin, end, tmp);
  storage = detail::unpack754(tmp);
  return result;
}

// the IEEE-754 conversion does not work for long double
// => fall back to string serialization (event though it sucks)
pointer read_range(pointer begin, pointer end, long double& storage) {
  std::string tmp;
  auto result = read_range(begin, end, tmp);
  std::istringstream iss{std::move(tmp)};
  iss >> storage;
  return result;
}

pointer read_range(pointer begin, pointer end, std::string& storage) {
  uint32_t str_size;
  begin = read_range(begin, end, str_size);
  range_check(begin, end, str_size);
  storage.clear();
  storage.reserve(str_size);
  pointer last = advanced(begin, str_size);
  copy(as_char_pointer(begin), as_char_pointer(last), back_inserter(storage));
  return advanced(begin, str_size);
}

template <class CharType, typename StringType>
pointer read_unicode_string(pointer begin, pointer end, StringType& str) {
  uint32_t str_size;
  begin = read_range(begin, end, str_size);
  str.reserve(str_size);
  for (size_t i = 0; i < str_size; ++i) {
    CharType c;
    begin = read_range(begin, end, c);
    str += static_cast<typename StringType::value_type>(c);
  }
  return begin;
}

pointer read_range(pointer begin, pointer end, atom_value& storage) {
  uint64_t tmp;
  auto result = read_range(begin, end, tmp);
  storage = static_cast<atom_value>(tmp);
  return result;
}

pointer read_range(pointer begin, pointer end, std::u16string& storage) {
  // char16_t is guaranteed to has *at least* 16 bytes,
  // but not to have *exactly* 16 bytes; thus use uint16_t
  return read_unicode_string<uint16_t>(begin, end, storage);
}

pointer read_range(pointer begin, pointer end, std::u32string& storage) {
  // char32_t is guaranteed to has *at least* 32 bytes,
  // but not to have *exactly* 32 bytes; thus use uint32_t
  return read_unicode_string<uint32_t>(begin, end, storage);
}

struct pt_reader : static_visitor<> {
  pointer begin;
  pointer end;
  pt_reader(pointer first, pointer last) : begin(first), end(last) {
    // nop
  }
  inline void operator()(none_t&) {
    // nop
  }
  inline void operator()(bool& value) {
    uint8_t intval;
    (*this)(intval);
    value = static_cast<bool>(intval);
  }
  template <class T>
  inline void operator()(T& value) {
    begin = read_range(begin, end, value);
  }
};

} // namespace <anonmyous>

binary_deserializer::binary_deserializer(const void* buf, size_t buf_size,
                                         actor_namespace* ns)
    : super(ns), pos_(buf), end_(advanced(buf, buf_size)) {
  // nop
}

binary_deserializer::binary_deserializer(const void* bbegin, const void* bend,
                                         actor_namespace* ns)
    : super(ns), pos_(bbegin), end_(bend) {
  // nop
}

const uniform_type_info* binary_deserializer::begin_object() {
  auto uti_map = detail::singletons::get_uniform_type_info_map();
  detail::uniform_type_info_map::pointer uti;
  uint16_t nr;
  pos_ = read_range(pos_, end_, nr);
  if (nr) {
    uti = uti_map->by_type_nr(nr);
  } else {
    std::string tname;
    pos_ = read_range(pos_, end_, tname);
    uti = uti_map->by_uniform_name(tname);
    if (! uti) {
      std::string err = "received type name \"";
      err += tname;
      err += "\" but no such type is known";
      throw std::runtime_error(err);
    }
  }
  return uti;
}

void binary_deserializer::end_object() {
  // nop
}

size_t binary_deserializer::begin_sequence() {
  CAF_LOG_TRACE("");
  static_assert(sizeof(size_t) >= sizeof(uint32_t),
                "sizeof(size_t) < sizeof(uint32_t)");
  uint32_t result;
  pos_ = read_range(pos_, end_, result);
  return static_cast<size_t>(result);
}

void binary_deserializer::end_sequence() {
  // nop
}

void binary_deserializer::read_value(primitive_variant& storage) {
  pt_reader ptr(pos_, end_);
  apply_visitor(ptr, storage);
  pos_ = ptr.begin;
}

void binary_deserializer::read_raw(size_t num_bytes, void* storage) {
  range_check(pos_, end_, num_bytes);
  memcpy(storage, pos_, num_bytes);
  pos_ = advanced(pos_, num_bytes);
}

} // namespace caf
