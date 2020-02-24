/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/serialized_size.hpp"

#include <iomanip>
#include <sstream>

#include "caf/error.hpp"
#include "caf/string_view.hpp"

namespace caf::detail {

error serialized_size_inspector::begin_object(type_id_t type) {
  return apply(type);
}

error serialized_size_inspector::end_object() {
  return none;
}

error serialized_size_inspector::begin_sequence(size_t list_size) {
  // Use varbyte encoding to compress sequence size on the wire.
  // For 64-bit values, the encoded representation cannot get larger than 10
  // bytes. A scratch space of 16 bytes suffices as upper bound.
  uint8_t buf[16];
  auto i = buf;
  auto x = static_cast<uint32_t>(list_size);
  while (x > 0x7f) {
    *i++ = (static_cast<uint8_t>(x) & 0x7f) | 0x80;
    x >>= 7;
  }
  *i++ = static_cast<uint8_t>(x) & 0x7f;
  result_ += static_cast<size_t>(i - buf);
  return none;
}

error serialized_size_inspector::end_sequence() {
  return none;
}

error serialized_size_inspector::apply(bool) {
  result_ += sizeof(uint8_t);
  return none;
}

error serialized_size_inspector::apply(int8_t x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(uint8_t x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(int16_t x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(uint16_t x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(int32_t x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(uint32_t x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(int64_t x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(uint64_t x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(float x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(double x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply(long double x) {
  // The IEEE-754 conversion does not work for long double
  // => fall back to string serialization (event though it sucks).
  std::ostringstream oss;
  oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
  auto tmp = oss.str();
  return apply(tmp);
}

error serialized_size_inspector::apply(string_view x) {
  begin_sequence(x.size());
  result_ += x.size();
  return end_sequence();
}

error serialized_size_inspector::apply(const std::u16string& x) {
  begin_sequence(x.size());
  result_ += x.size() * sizeof(uint16_t);
  return end_sequence();
}

error serialized_size_inspector::apply(const std::u32string& x) {
  begin_sequence(x.size());
  result_ += x.size() * sizeof(uint32_t);
  return end_sequence();
}

error serialized_size_inspector::apply(span<const byte> x) {
  result_ += x.size();
  return none;
}

error serialized_size_inspector::apply(const std::vector<bool>& xs) {
  begin_sequence(xs.size());
  result_ += (xs.size() + static_cast<size_t>(xs.size() % 8 != 0)) / 8;
  return end_sequence();
}

} // namespace caf::detail
