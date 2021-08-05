// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/binary_deserializer.hpp"

#include <iomanip>
#include <sstream>
#include <type_traits>

#include "caf/actor_system.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/sec.hpp"

namespace caf {

namespace {

template <class T>
bool int_value(binary_deserializer& source, T& x) {
  auto tmp = std::make_unsigned_t<T>{};
  if (source.value(as_writable_bytes(make_span(&tmp, 1)))) {
    x = static_cast<T>(detail::from_network_order(tmp));
    return true;
  } else {
    return false;
  }
}

template <class T>
bool float_value(binary_deserializer& source, T& x) {
  auto tmp = typename detail::ieee_754_trait<T>::packed_type{};
  if (int_value(source, tmp)) {
    x = detail::unpack754(tmp);
    return true;
  } else {
    return false;
  }
}

// Does not perform any range checks.
template <class T>
void unsafe_int_value(binary_deserializer& source, T& x) {
  std::make_unsigned_t<T> tmp;
  memcpy(&tmp, source.current(), sizeof(tmp));
  source.skip(sizeof(tmp));
  x = static_cast<T>(detail::from_network_order(tmp));
}

} // namespace

binary_deserializer::binary_deserializer(actor_system& sys) noexcept
  : context_(sys.dummy_execution_unit()) {
  // nop
}

bool binary_deserializer::fetch_next_object_type(type_id_t& type) noexcept {
  type = invalid_type_id;
  emplace_error(sec::unsupported_operation,
                "the default binary format does not embed type information");
  return false;
}

bool binary_deserializer::begin_sequence(size_t& list_size) noexcept {
  // Use varbyte encoding to compress sequence size on the wire.
  uint32_t x = 0;
  int n = 0;
  uint8_t low7 = 0;
  do {
    if (!value(low7))
      return false;
    x |= static_cast<uint32_t>((low7 & 0x7F)) << (7 * n);
    ++n;
  } while (low7 & 0x80);
  list_size = x;
  return true;
}

void binary_deserializer::skip(size_t num_bytes) {
  if (num_bytes > remaining())
    CAF_RAISE_ERROR("cannot skip past the end");
  current_ += num_bytes;
}

void binary_deserializer::reset(span<const std::byte> bytes) noexcept {
  current_ = bytes.data();
  end_ = current_ + bytes.size();
}

bool binary_deserializer::begin_field(string_view, bool& is_present) noexcept {
  auto tmp = uint8_t{0};
  if (!value(tmp))
    return false;
  is_present = static_cast<bool>(tmp);
  return true;
}

namespace {

template <class T>
constexpr size_t max_value = static_cast<size_t>(std::numeric_limits<T>::max());

} // namespace

bool binary_deserializer::begin_field(string_view, span<const type_id_t> types,
                                      size_t& index) noexcept {
  auto f = [&](auto tmp) {
    if (!value(tmp))
      return false;
    if (tmp < 0 || static_cast<size_t>(tmp) >= types.size()) {
      emplace_error(sec::invalid_field_type,
                    "received type index out of bounds");
      return false;
    }
    index = static_cast<size_t>(tmp);
    return true;
  };
  if (types.size() < max_value<int8_t>) {
    return f(int8_t{0});
  } else if (types.size() < max_value<int16_t>) {
    return f(int16_t{0});
  } else if (types.size() < max_value<int32_t>) {
    return f(int32_t{0});
  } else {
    return f(int64_t{0});
  }
}

bool binary_deserializer::begin_field(string_view, bool& is_present,
                                      span<const type_id_t> types,
                                      size_t& index) noexcept {
  auto f = [&](auto tmp) {
    if (!value(tmp))
      return false;
    if (tmp < 0) {
      is_present = false;
      return true;
    }
    if (static_cast<size_t>(tmp) >= types.size()) {
      emplace_error(sec::invalid_field_type,
                    "received type index out of bounds");
      return false;
    }
    is_present = true;
    index = static_cast<size_t>(tmp);
    return true;
  };
  if (types.size() < max_value<int8_t>) {
    return f(int8_t{0});
  } else if (types.size() < max_value<int16_t>) {
    return f(int16_t{0});
  } else if (types.size() < max_value<int32_t>) {
    return f(int32_t{0});
  } else {
    return f(int64_t{0});
  }
}

bool binary_deserializer::value(bool& x) noexcept {
  int8_t tmp = 0;
  if (!value(tmp))
    return false;
  x = tmp != 0;
  return true;
}

bool binary_deserializer::value(std::byte& x) noexcept {
  if (range_check(1)) {
    x = *current_++;
    return true;
  }
  emplace_error(sec::end_of_stream);
  return false;
}

bool binary_deserializer::value(int8_t& x) noexcept {
  if (range_check(1)) {
    x = static_cast<int8_t>(*current_++);
    return true;
  }
  emplace_error(sec::end_of_stream);
  return false;
}

bool binary_deserializer::value(uint8_t& x) noexcept {
  if (range_check(1)) {
    x = static_cast<uint8_t>(*current_++);
    return true;
  }
  emplace_error(sec::end_of_stream);
  return false;
}

bool binary_deserializer::value(int16_t& x) noexcept {
  return int_value(*this, x);
}

bool binary_deserializer::value(uint16_t& x) noexcept {
  return int_value(*this, x);
}

bool binary_deserializer::value(int32_t& x) noexcept {
  return int_value(*this, x);
}

bool binary_deserializer::value(uint32_t& x) noexcept {
  return int_value(*this, x);
}

bool binary_deserializer::value(int64_t& x) noexcept {
  return int_value(*this, x);
}

bool binary_deserializer::value(uint64_t& x) noexcept {
  return int_value(*this, x);
}

bool binary_deserializer::value(float& x) noexcept {
  return float_value(*this, x);
}

bool binary_deserializer::value(double& x) noexcept {
  return float_value(*this, x);
}

bool binary_deserializer::value(long double& x) {
  // TODO: Our IEEE-754 conversion currently does not work for long double. The
  //       standard does not guarantee a fixed representation for this type, but
  //       on X86 we can usually rely on 80-bit precision. For now, we fall back
  //       to string conversion.
  std::string tmp;
  if (!value(tmp))
    return false;
  std::istringstream iss{std::move(tmp)};
  if (iss >> x)
    return true;
  emplace_error(sec::invalid_argument);
  return false;
}

bool binary_deserializer::value(span<std::byte> x) noexcept {
  if (!range_check(x.size())) {
    emplace_error(sec::end_of_stream);
    return false;
  }
  memcpy(x.data(), current_, x.size());
  current_ += x.size();
  return true;
}

bool binary_deserializer::value(std::string& x) {
  x.clear();
  size_t str_size = 0;
  if (!begin_sequence(str_size))
    return false;
  if (!range_check(str_size)) {
    emplace_error(sec::end_of_stream);
    return false;
  }
  x.assign(reinterpret_cast<const char*>(current_), str_size);
  current_ += str_size;
  return end_sequence();
}

bool binary_deserializer::value(std::u16string& x) {
  x.clear();
  size_t str_size = 0;
  if (!begin_sequence(str_size))
    return false;
  if (!range_check(str_size * sizeof(uint16_t))) {
    emplace_error(sec::end_of_stream);
    return false;
  }
  for (size_t i = 0; i < str_size; ++i) {
    // The standard does not guarantee that char16_t is exactly 16 bits.
    uint16_t tmp;
    unsafe_int_value(*this, tmp);
    x.push_back(static_cast<char16_t>(tmp));
  }
  return end_sequence();
}

bool binary_deserializer::value(std::u32string& x) {
  x.clear();
  size_t str_size = 0;
  if (!begin_sequence(str_size))
    return false;
  if (!range_check(str_size * sizeof(uint32_t))) {
    emplace_error(sec::end_of_stream);
    return false;
  }
  for (size_t i = 0; i < str_size; ++i) {
    // The standard does not guarantee that char32_t is exactly 32 bits.
    uint32_t tmp;
    unsafe_int_value(*this, tmp);
    x.push_back(static_cast<char32_t>(tmp));
  }
  return end_sequence();
}

bool binary_deserializer::value(std::vector<bool>& x) {
  x.clear();
  size_t len = 0;
  if (!begin_sequence(len))
    return false;
  if (len == 0)
    return end_sequence();
  size_t blocks = len / 8;
  for (size_t block = 0; block < blocks; ++block) {
    uint8_t tmp = 0;
    if (!value(tmp))
      return false;
    x.emplace_back((tmp & 0b1000'0000) != 0);
    x.emplace_back((tmp & 0b0100'0000) != 0);
    x.emplace_back((tmp & 0b0010'0000) != 0);
    x.emplace_back((tmp & 0b0001'0000) != 0);
    x.emplace_back((tmp & 0b0000'1000) != 0);
    x.emplace_back((tmp & 0b0000'0100) != 0);
    x.emplace_back((tmp & 0b0000'0010) != 0);
    x.emplace_back((tmp & 0b0000'0001) != 0);
  }
  auto trailing_block_size = len % 8;
  if (trailing_block_size > 0) {
    uint8_t tmp = 0;
    if (!value(tmp))
      return false;
    switch (trailing_block_size) {
      case 7:
        x.emplace_back((tmp & 0b0100'0000) != 0);
        [[fallthrough]];
      case 6:
        x.emplace_back((tmp & 0b0010'0000) != 0);
        [[fallthrough]];
      case 5:
        x.emplace_back((tmp & 0b0001'0000) != 0);
        [[fallthrough]];
      case 4:
        x.emplace_back((tmp & 0b0000'1000) != 0);
        [[fallthrough]];
      case 3:
        x.emplace_back((tmp & 0b0000'0100) != 0);
        [[fallthrough]];
      case 2:
        x.emplace_back((tmp & 0b0000'0010) != 0);
        [[fallthrough]];
      case 1:
        x.emplace_back((tmp & 0b0000'0001) != 0);
        [[fallthrough]];
      default:
        break;
    }
  }
  return end_sequence();
}

} // namespace caf
