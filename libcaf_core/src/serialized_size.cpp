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

namespace caf {
namespace detail {

error serialized_size_inspector::begin_object(uint16_t& nr, std::string& name) {
  if (nr != 0)
    return apply(nr);
  if (auto err = apply(nr))
    return err;
  return apply(name);
}

error serialized_size_inspector::end_object() {
  return none;
}

error serialized_size_inspector::begin_sequence(size_t& list_size) {
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
  apply_raw(static_cast<size_t>(i - buf), buf);
  return none;
}

error serialized_size_inspector::end_sequence() {
  return none;
}

error serialized_size_inspector::apply_raw(size_t num_bytes, void*) {
  result_ += num_bytes;
  return none;
}

error serialized_size_inspector::apply_impl(int8_t& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(uint8_t& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(int16_t& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(uint16_t& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(int32_t& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(uint32_t& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(int64_t& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(uint64_t& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(float& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(double& x) {
  result_ += sizeof(x);
  return none;
}

error serialized_size_inspector::apply_impl(long double& x) {
  // The IEEE-754 conversion does not work for long double
  // => fall back to string serialization (event though it sucks).
  std::ostringstream oss;
  oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
  auto tmp = oss.str();
  return apply_impl(tmp);
}

error serialized_size_inspector::apply_impl(std::string& x) {
  size_t str_size = x.size();
  begin_sequence(str_size);
  result_ += x.size();
  end_sequence();
  return none;
}

error serialized_size_inspector::apply_impl(std::u16string& x) {
  size_t str_size = x.size();
  begin_sequence(str_size);
  result_ += x.size() * sizeof(uint16_t);
  end_sequence();
  return none;
}

error serialized_size_inspector::apply_impl(std::u32string& x) {
  size_t str_size = x.size();
  begin_sequence(str_size);
  result_ += x.size() * sizeof(uint32_t);
  end_sequence();
  return none;
}

} // namespace detail
} // namespace caf
