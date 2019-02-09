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

#include "caf/binary_deserializer.hpp"

#include <iomanip>
#include <sstream>
#include <type_traits>

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"

namespace caf {

namespace {

template <class T>
error apply_int(binary_deserializer& bs, T& x) {
  typename std::make_unsigned<T>::type tmp;
  if (auto err = bs.apply_raw(sizeof(T), &tmp))
    return err;
  x = static_cast<T>(detail::from_network_order(tmp));
  return none;
}

template <class T>
error apply_float(binary_deserializer& bs, T& x) {
  typename detail::ieee_754_trait<T>::packed_type tmp;
  if (auto err = apply_int(bs, tmp))
    return err;
  x = detail::unpack754(tmp);
  return none;
}

} // namespace <anonmyous>

binary_deserializer::binary_deserializer(actor_system& sys, const char* buf,
                                         size_t buf_size)
  : super(sys), pos_(buf), end_(buf + buf_size) {
  // nop
}

binary_deserializer::binary_deserializer(execution_unit* ctx, const char* buf,
                                         size_t buf_size)
  : super(ctx), pos_(buf), end_(buf + buf_size) {
  // nop
}

binary_deserializer::binary_deserializer(actor_system& sys, const buffer& buf)
  : binary_deserializer(sys, buf.data(), buf.size()) {
  // nop
}

binary_deserializer::binary_deserializer(execution_unit* ctx, const buffer& buf)
  : binary_deserializer(ctx, buf.data(), buf.size()) {
  // nop
}

error binary_deserializer::begin_object(uint16_t& nr, std::string& name) {
  if (auto err = apply(nr))
    return err;
  if (nr != 0)
    return none;
  return apply(name);
}

error binary_deserializer::end_object() {
  return none;
}

error binary_deserializer::begin_sequence(size_t& list_size) {
  auto s = static_cast<uint32_t>(list_size);
  if (auto err = apply(s))
    return err;
  list_size = s;
  return none;
}

error binary_deserializer::end_sequence() {
  return none;
}

error binary_deserializer::apply_raw(size_t num_bytes, void* storage) {
  if (!range_check(num_bytes))
    return sec::end_of_stream;
  memcpy(storage, pos_, num_bytes);
  pos_ += num_bytes;
  return none;
}

error binary_deserializer::apply_impl(int8_t& x) {
  return apply_raw(sizeof(int8_t), &x);
}

error binary_deserializer::apply_impl(uint8_t& x) {
  return apply_raw(sizeof(uint8_t), &x);
}

error binary_deserializer::apply_impl(int16_t& x) {
  return apply_int(*this, x);
}

error binary_deserializer::apply_impl(uint16_t& x) {
  return apply_int(*this, x);
}

error binary_deserializer::apply_impl(int32_t& x) {
  return apply_int(*this, x);
}

error binary_deserializer::apply_impl(uint32_t& x) {
  return apply_int(*this, x);
}

error binary_deserializer::apply_impl(int64_t& x) {
  return apply_int(*this, x);
}

error binary_deserializer::apply_impl(uint64_t& x) {
  return apply_int(*this, x);
}

error binary_deserializer::apply_impl(float& x) {
  return apply_float(*this, x);
}

error binary_deserializer::apply_impl(double& x) {
  return apply_float(*this, x);
}

error binary_deserializer::apply_impl(long double& x) {
  // The IEEE-754 conversion does not work for long double
  // => fall back to string serialization (even though it sucks).
  std::string tmp;
  if (auto err = apply(tmp))
    return err;
  std::istringstream iss{std::move(tmp)};
  iss >> x;
  return none;
}
error binary_deserializer::apply_impl(std::string& x) {
  size_t str_size;
  if (auto err = begin_sequence(str_size))
    return err;
  if (!range_check(str_size))
    return sec::end_of_stream;
  x.assign(pos_, pos_ + str_size);
  pos_ += str_size;
  return end_sequence();
}

error binary_deserializer::apply_impl(std::u16string& x) {
  auto str_size = x.size();
  if (auto err = begin_sequence(str_size))
    return err;
  for (size_t i = 0; i < str_size; ++i) {
    // The standard does not guarantee that char16_t is exactly 16 bits.
    uint16_t tmp;
    if (auto err = apply_int(*this, tmp))
      return err;
    x.push_back(static_cast<char16_t>(tmp));
  }
  return none;
}

error binary_deserializer::apply_impl(std::u32string& x) {
  auto str_size = x.size();
  if (auto err = begin_sequence(str_size))
    return err;
  for (size_t i = 0; i < str_size; ++i) {
    // The standard does not guarantee that char32_t is exactly 32 bits.
    uint32_t tmp;
    if (auto err = apply_int(*this, tmp))
      return err;
    x.push_back(static_cast<char32_t>(tmp));
  }
  return none;
}

} // namespace caf
