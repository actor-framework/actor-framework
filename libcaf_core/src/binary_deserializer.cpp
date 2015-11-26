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

#include <limits>
#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iterator>
#include <exception>
#include <stdexcept>
#include <type_traits>

#include "caf/logger.hpp"
#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"

namespace caf {

namespace {

using pointer = const void*;

const char* as_char_pointer(pointer ptr) {
  return reinterpret_cast<const char*>(ptr);
}

template <class Distance>
pointer advanced(pointer ptr, Distance num_bytes) {
  return reinterpret_cast<const char*>(ptr) + num_bytes;
}

inline void range_check(pointer begin, pointer end, size_t read_size) {
  if (advanced(begin, read_size) > end) {
    CAF_LOG_ERROR("range_check failed");
    throw std::out_of_range("binary_deserializer::read_range()");
  }
}

template <class T>
void apply_int(binary_deserializer& bd, T& x) {
  T tmp;
  bd.apply_raw(sizeof(T), &tmp);
  x = detail::from_network_order(tmp);
}

template <class T>
void apply_float(binary_deserializer& bd, T& x) {
  typename detail::ieee_754_trait<T>::packed_type tmp;
  apply_int(bd, tmp);
  x = detail::unpack754(tmp);
}

} // namespace <anonmyous>

binary_deserializer::binary_deserializer(actor_system& sys,
                                         const void* buf, size_t buf_size)
  : binary_deserializer(sys, buf, advanced(buf, buf_size)) {
  // nop
}

binary_deserializer::binary_deserializer(execution_unit* ctx,
                                         const void* buf,
                                         size_t buf_size)
    : binary_deserializer(ctx, buf, advanced(buf, buf_size)) {
  // nop
}

binary_deserializer::binary_deserializer(actor_system& sys,
                                         const void* first, const void* last)
    : binary_deserializer(sys.dummy_execution_unit(), first, last) {
  // nop
}

binary_deserializer::binary_deserializer(execution_unit* ctx,
                                         const void* first, const void* last)
    : deserializer(ctx),
      pos_(first),
      end_(last) {
  // nop
}

bool binary_deserializer::at_end() const {
  return pos_ == end_;
}

/// with same semantics as `strncmp(this->pos_, buf, num_bytes)`.
bool binary_deserializer::buf_equals(const void* buf, size_t num_bytes) {
  auto bytes_left = static_cast<size_t>(std::distance(as_char_pointer(pos_),
                                                      as_char_pointer(end_)));
  if (bytes_left < num_bytes)
    return false;
  return strncmp(as_char_pointer(pos_), as_char_pointer(buf), num_bytes) == 0;
}

binary_deserializer& binary_deserializer::advance(ptrdiff_t num_bytes) {
  pos_ = advanced(pos_, num_bytes);
  return *this;
}

void binary_deserializer::begin_object(uint16_t& nr, std::string& name) {
  apply_int(*this, nr);
  if (nr == 0)
    apply(name);
}

void binary_deserializer::end_object() {
  // nop
}

void binary_deserializer::begin_sequence(size_t& result) {
  CAF_LOG_TRACE("");
  static_assert(sizeof(size_t) >= sizeof(uint32_t),
                "sizeof(size_t) < sizeof(uint32_t)");
  uint32_t tmp;
  apply_int(*this, tmp);
  result = static_cast<size_t>(tmp);
}

void binary_deserializer::end_sequence() {
  // nop
}

void binary_deserializer::apply_raw(size_t num_bytes, void* storage) {
  range_check(pos_, end_, num_bytes);
  memcpy(storage, pos_, num_bytes);
  pos_ = advanced(pos_, num_bytes);
}

void binary_deserializer::apply_builtin(builtin type, void* val) {
  CAF_ASSERT(val != nullptr);
  switch (type) {
    case i8_v:
    case u8_v:
      apply_raw(sizeof(uint8_t), val);
      break;
    case i16_v:
    case u16_v:
      apply_int(*this, *reinterpret_cast<uint16_t*>(val));
      break;
    case i32_v:
    case u32_v:
      apply_int(*this, *reinterpret_cast<uint32_t*>(val));
      break;
    case i64_v:
    case u64_v:
      apply_int(*this, *reinterpret_cast<uint64_t*>(val));
      break;
    case float_v:
      apply_float(*this, *reinterpret_cast<float*>(val));
      break;
    case double_v:
      apply_float(*this, *reinterpret_cast<double*>(val));
      break;
    case ldouble_v: {
      // the IEEE-754 conversion does not work for long double
      // => fall back to string serialization (event though it sucks)
      std::string tmp;
      apply(tmp);
      std::istringstream iss{std::move(tmp)};
      iss >> *reinterpret_cast<long double*>(val);
      break;
    }
    case string8_v: {
      auto& str = *reinterpret_cast<std::string*>(val);
      uint32_t str_size;
      apply_int(*this, str_size);
      range_check(pos_, end_, str_size);
      str.clear();
      str.reserve(str_size);
      auto last = advanced(pos_, str_size);
      std::copy(reinterpret_cast<const char*>(pos_),
                reinterpret_cast<const char*>(last),
                std::back_inserter(str));
      pos_ = last;
      break;
    }
    case string16_v: {
      auto& str = *reinterpret_cast<std::string*>(val);
      uint32_t str_size;
      apply_int(*this, str_size);
      range_check(pos_, end_, str_size * sizeof(uint16_t));
      str.clear();
      str.reserve(str_size);
      auto last = advanced(pos_, str_size);
      std::copy(reinterpret_cast<const uint16_t*>(pos_),
                reinterpret_cast<const uint16_t*>(last),
                std::back_inserter(str));
      pos_ = last;
      break;
    }
    case string32_v: {
      auto& str = *reinterpret_cast<std::string*>(val);
      uint32_t str_size;
      apply_int(*this, str_size);
      range_check(pos_, end_, str_size * sizeof(uint32_t));
      str.clear();
      str.reserve(str_size);
      auto last = advanced(pos_, str_size);
      std::copy(reinterpret_cast<const uint32_t*>(pos_),
                reinterpret_cast<const uint32_t*>(last),
                std::back_inserter(str));
      pos_ = last;
      break;
    }
  }
}

} // namespace caf
