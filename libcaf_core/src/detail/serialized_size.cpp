// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/serialized_size.hpp"

#include <iomanip>
#include <sstream>

#include "caf/error.hpp"
#include "caf/string_view.hpp"

namespace caf::detail {

bool serialized_size_inspector::begin_object(type_id_t, string_view) {
  return true;
}

bool serialized_size_inspector::end_object() {
  return true;
}

bool serialized_size_inspector::begin_field(string_view, bool) {
  result += 1;
  return true;
}

namespace {

template <class T>
constexpr size_t max_value = static_cast<size_t>(std::numeric_limits<T>::max());

} // namespace

bool serialized_size_inspector::begin_field(string_view) {
  return true;
}

bool serialized_size_inspector::begin_field(string_view,
                                            span<const type_id_t> types,
                                            size_t) {
  if (types.size() < max_value<int8_t>) {
    result += sizeof(int8_t);
  } else if (types.size() < max_value<int16_t>) {
    result += sizeof(int16_t);
  } else if (types.size() < max_value<int32_t>) {
    result += sizeof(int32_t);
  } else {
    result += sizeof(int64_t);
  }
  return true;
}

bool serialized_size_inspector::begin_field(string_view name, bool,
                                            span<const type_id_t> types,
                                            size_t index) {
  return begin_field(name, types, index);
}

bool serialized_size_inspector::end_field() {
  return true;
}

bool serialized_size_inspector::begin_tuple(size_t) {
  return true;
}

bool serialized_size_inspector::end_tuple() {
  return true;
}

bool serialized_size_inspector::begin_sequence(size_t list_size) {
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
  result += static_cast<size_t>(i - buf);
  return true;
}

bool serialized_size_inspector::end_sequence() {
  return true;
}

bool serialized_size_inspector::value(byte x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(bool) {
  result += sizeof(uint8_t);
  return true;
}

bool serialized_size_inspector::value(int8_t x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(uint8_t x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(int16_t x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(uint16_t x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(int32_t x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(uint32_t x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(int64_t x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(uint64_t x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(float x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(double x) {
  result += sizeof(x);
  return true;
}

bool serialized_size_inspector::value(long double x) {
  // The IEEE-754 conversion does not work for long double
  // => fall back to string serialization (event though it sucks).
  std::ostringstream oss;
  oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
  auto tmp = oss.str();
  return value(tmp);
}

bool serialized_size_inspector::value(string_view x) {
  CAF_IGNORE_UNUSED(begin_sequence(x.size()));
  result += x.size();
  return end_sequence();
}

bool serialized_size_inspector::value(const std::u16string& x) {
  CAF_IGNORE_UNUSED(begin_sequence(x.size()));
  result += x.size() * sizeof(uint16_t);
  return end_sequence();
}

bool serialized_size_inspector::value(const std::u32string& x) {
  CAF_IGNORE_UNUSED(begin_sequence(x.size()));
  result += x.size() * sizeof(uint32_t);
  return end_sequence();
}

bool serialized_size_inspector::value(span<const byte> x) {
  result += x.size();
  return true;
}

bool serialized_size_inspector::list(const std::vector<bool>& xs) {
  CAF_IGNORE_UNUSED(begin_sequence(xs.size()));
  result += (xs.size() + static_cast<size_t>(xs.size() % 8 != 0)) / 8;
  return end_sequence();
}

} // namespace caf::detail
