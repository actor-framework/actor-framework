/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/binary_serializer.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"

namespace caf {

namespace {

template <class T>
error apply_int(binary_serializer& bs, T x) {
  auto y = detail::to_network_order(x);
  return bs.apply_raw(sizeof(T), &y);
}

} // namespace <anonmyous>

binary_serializer::binary_serializer(actor_system& sys, buffer& buf)
  : super(sys), buf_(buf), write_pos_(buf_.size()) {
  // nop
}

binary_serializer::binary_serializer(execution_unit* ctx, buffer& buf)
  : super(ctx), buf_(buf), write_pos_(buf_.size()) {
  // nop
}

void binary_serializer::seek(size_t offset) {
  write_pos_ = offset;
}

void binary_serializer::skip(size_t num_bytes) {
  auto remaining = buf_.size() - write_pos_;
  if (remaining < num_bytes)
    buf_.insert(buf_.end(), num_bytes - remaining, 0);
  write_pos_ += num_bytes;
}

error binary_serializer::begin_object(uint16_t& nr, std::string& name) {
  if (nr != 0)
    return apply(nr);
  if (auto err = apply(nr))
    return err;
  return apply(name);
}

error binary_serializer::end_object() {
  return none;
}

error binary_serializer::begin_sequence(size_t& list_size) {
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

error binary_serializer::end_sequence() {
  return none;
}

error binary_serializer::apply_raw(size_t num_bytes, void* data) {
  CAF_ASSERT(write_pos_ <= buf_.size());
  auto ptr = reinterpret_cast<char*>(data);
  auto buf_size = buf_.size();
  if (write_pos_ == buf_size) {
    buf_.insert(buf_.end(), ptr, ptr + num_bytes);
  } else if (write_pos_ + num_bytes <= buf_size) {
    memcpy(buf_.data() + write_pos_, ptr, num_bytes);
  } else {
    auto remaining = buf_size - write_pos_;
    CAF_ASSERT(remaining < num_bytes);
    memcpy(buf_.data() + write_pos_, ptr, remaining);
    buf_.insert(buf_.end(), ptr + remaining, ptr + num_bytes);
  }
  write_pos_ += num_bytes;
  CAF_ASSERT(write_pos_ <= buf_.size());
  return none;
}

error binary_serializer::apply_impl(int8_t& x) {
  return apply_raw(sizeof(int8_t), &x);
}

error binary_serializer::apply_impl(uint8_t& x) {
  return apply_raw(sizeof(uint8_t), &x);
}

error binary_serializer::apply_impl(int16_t& x) {
  return apply_int(*this, static_cast<uint16_t>(x));
}

error binary_serializer::apply_impl(uint16_t& x) {
  return apply_int(*this, x);
}

error binary_serializer::apply_impl(int32_t& x) {
  return apply_int(*this, static_cast<uint32_t>(x));
}

error binary_serializer::apply_impl(uint32_t& x) {
  return apply_int(*this, x);
}

error binary_serializer::apply_impl(int64_t& x) {
  return apply_int(*this, static_cast<uint64_t>(x));
}

error binary_serializer::apply_impl(uint64_t& x) {
  return apply_int(*this, x);
}

error binary_serializer::apply_impl(float& x) {
  return apply_int(*this, detail::pack754(x));
}

error binary_serializer::apply_impl(double& x) {
  return apply_int(*this, detail::pack754(x));
}

error binary_serializer::apply_impl(long double& x) {
  // The IEEE-754 conversion does not work for long double
  // => fall back to string serialization (event though it sucks).
  std::ostringstream oss;
  oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
  auto tmp = oss.str();
  return apply_impl(tmp);
}
error binary_serializer::apply_impl(std::string& x) {
  auto str_size = x.size();
  if (str_size == 0)
    return error::eval([&] { return begin_sequence(str_size); },
                       [&] { return end_sequence(); });
  auto data = const_cast<char*>(x.c_str());
  return error::eval([&] { return begin_sequence(str_size); },
                     [&] { return apply_raw(str_size, data); },
                     [&] { return end_sequence(); });
}

error binary_serializer::apply_impl(std::u16string& x) {
  auto str_size = x.size();
  if (auto err = begin_sequence(str_size))
    return err;
  for (auto c : x) {
    // The standard does not guarantee that char16_t is exactly 16 bits.
    if (auto err = apply_int(*this, static_cast<uint16_t>(c)))
      return err;
  }
  return none;
}

error binary_serializer::apply_impl(std::u32string& x) {
  auto str_size = x.size();
  if (auto err = begin_sequence(str_size))
    return err;
  for (auto c : x) {
    // The standard does not guarantee that char16_t is exactly 16 bits.
    if (auto err = apply_int(*this, static_cast<uint32_t>(c)))
      return err;
  }
  return none;
}

} // namespace caf
