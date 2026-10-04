// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/binary_serializer.hpp"

#include "caf/actor_handle_codec.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/default_actor_handle_codec.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"
#include "caf/type_id_list.hpp"

#include <cstring>
#include <iomanip>
#include <limits>
#include <span>
#include <sstream>

namespace {

template <class T>
inline constexpr auto max_value
  = static_cast<size_t>(std::numeric_limits<T>::max());

template <class T>
T compress_index(bool is_present, size_t value) {
  return is_present ? static_cast<T>(value) : T{-1};
}

size_t skip_bytes(caf::byte_buffer& buf, size_t num_bytes) {
  auto offset = buf.size();
  buf.insert(buf.end(), num_bytes, std::byte{0});
  return offset;
}

} // namespace

namespace caf {

binary_serializer_base::~binary_serializer_base() noexcept {
  // nop
}

template <class T>
bool binary_serializer_base::int_value(T x) {
  using unsigned_type = detail::squashed_int_t<std::make_unsigned_t<T>>;
  auto y = detail::to_network_order(static_cast<unsigned_type>(x));
  return value(as_bytes(std::span{&y, 1}));
}

bool binary_serializer_base::has_human_readable_format() const noexcept {
  return false;
}

void binary_serializer_base::set_error(error stop_reason) {
  err_ = std::move(stop_reason);
}

error& binary_serializer_base::get_error() noexcept {
  return err_;
}

bool binary_serializer_base::begin_object(type_id_t,
                                          std::string_view) noexcept {
  return true;
}

bool binary_serializer_base::end_object() {
  return true;
}

bool binary_serializer_base::begin_field(std::string_view) noexcept {
  return true;
}

bool binary_serializer_base::begin_field(std::string_view, bool is_present) {
  auto val = static_cast<uint8_t>(is_present);
  return value(val);
}

bool binary_serializer_base::begin_field(std::string_view,
                                         std::span<const type_id_t> types,
                                         size_t index) {
  CAF_ASSERT(index < types.size());
  if (types.size() < max_value<int8_t>) {
    return value(static_cast<int8_t>(index));
  } else if (types.size() < max_value<int16_t>) {
    return value(static_cast<int16_t>(index));
  } else if (types.size() < max_value<int32_t>) {
    return value(static_cast<int32_t>(index));
  } else {
    return value(static_cast<int64_t>(index));
  }
}

bool binary_serializer_base::begin_field(std::string_view, bool is_present,
                                         std::span<const type_id_t> types,
                                         size_t index) {
  CAF_ASSERT(!is_present || index < types.size());
  if (types.size() < max_value<int8_t>) {
    return value(compress_index<int8_t>(is_present, index));
  } else if (types.size() < max_value<int16_t>) {
    return value(compress_index<int16_t>(is_present, index));
  } else if (types.size() < max_value<int32_t>) {
    return value(compress_index<int32_t>(is_present, index));
  } else {
    return value(compress_index<int64_t>(is_present, index));
  }
}

bool binary_serializer_base::end_field() {
  return true;
}

bool binary_serializer_base::begin_tuple(size_t) {
  return true;
}

bool binary_serializer_base::end_tuple() {
  return true;
}

bool binary_serializer_base::begin_key_value_pair() {
  return true;
}

bool binary_serializer_base::end_key_value_pair() {
  return true;
}

bool binary_serializer_base::begin_sequence(size_t list_size) {
  // Use varbyte encoding to compress sequence size on the wire.
  // For 64-bit values, the encoded representation cannot get larger than 10
  // bytes. A scratch space of 16 bytes suffices as upper bound.
  uint8_t bytes_buf[16];
  auto i = bytes_buf;
  auto x = static_cast<uint32_t>(list_size);
  while (x > 0x7f) {
    *i++ = (static_cast<uint8_t>(x) & 0x7f) | 0x80;
    x >>= 7;
  }
  *i++ = static_cast<uint8_t>(x) & 0x7f;
  return value(
    as_bytes(std::span{bytes_buf, static_cast<size_t>(i - bytes_buf)}));
}

bool binary_serializer_base::end_sequence() {
  return true;
}

bool binary_serializer_base::begin_associative_array(size_t size) {
  return begin_sequence(size);
}

bool binary_serializer_base::end_associative_array() {
  return end_sequence();
}

bool binary_serializer_base::value(const_byte_span x) {
  do_append(x.data(), x.size());
  return true;
}

bool binary_serializer_base::value(std::byte x) {
  do_append(&x, 1);
  return true;
}

bool binary_serializer_base::value(bool x) {
  return value(static_cast<uint8_t>(x));
}

bool binary_serializer_base::value(int8_t x) {
  return value(static_cast<std::byte>(x));
}

bool binary_serializer_base::value(uint8_t x) {
  return value(static_cast<std::byte>(x));
}

bool binary_serializer_base::value(int16_t x) {
  return int_value(x);
}

bool binary_serializer_base::value(uint16_t x) {
  return int_value(x);
}

bool binary_serializer_base::value(int32_t x) {
  return int_value(x);
}

bool binary_serializer_base::value(uint32_t x) {
  return int_value(x);
}

bool binary_serializer_base::value(int64_t x) {
  return int_value(x);
}

bool binary_serializer_base::value(uint64_t x) {
  return int_value(x);
}

bool binary_serializer_base::value(float x) {
  return int_value(detail::pack754(x));
}

bool binary_serializer_base::value(double x) {
  return int_value(detail::pack754(x));
}

bool binary_serializer_base::value(long double x) {
  // TODO: Our IEEE-754 conversion currently does not work for long double.
  //       The standard does not guarantee a fixed representation for this
  //       type, but on X86 we can usually rely on 80-bit precision. For now,
  //       we fall back to string conversion.
  std::ostringstream oss;
  oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
  auto tmp = oss.str();
  return value(tmp);
}

bool binary_serializer_base::value(std::string_view x) {
  if (!begin_sequence(x.size()))
    return false;
  value(as_bytes(std::span{x}));
  return end_sequence();
}

bool binary_serializer_base::value(const std::u16string& x) {
  auto str_size = x.size();
  if (!begin_sequence(str_size))
    return false;
  // The standard does not guarantee that char16_t is exactly 16 bits.
  for (auto c : x)
    int_value(static_cast<uint16_t>(c));
  return end_sequence();
}

bool binary_serializer_base::value(const std::u32string& x) {
  auto str_size = x.size();
  if (!begin_sequence(str_size))
    return false;
  // The standard does not guarantee that char32_t is exactly 32 bits.
  for (auto c : x)
    int_value(static_cast<uint32_t>(c));
  return end_sequence();
}

bool binary_serializer_base::value(const std::vector<bool>& x) {
  auto len = x.size();
  if (!begin_sequence(len))
    return false;
  if (len == 0)
    return end_sequence();
  size_t pos = 0;
  size_t blocks = len / 8;
  for (size_t block = 0; block < blocks; ++block) {
    uint8_t tmp = 0;
    if (x[pos++])
      tmp |= 0b1000'0000;
    if (x[pos++])
      tmp |= 0b0100'0000;
    if (x[pos++])
      tmp |= 0b0010'0000;
    if (x[pos++])
      tmp |= 0b0001'0000;
    if (x[pos++])
      tmp |= 0b0000'1000;
    if (x[pos++])
      tmp |= 0b0000'0100;
    if (x[pos++])
      tmp |= 0b0000'0010;
    if (x[pos++])
      tmp |= 0b0000'0001;
    if (!value(tmp)) {
      return false;
    }
  }
  auto trailing_block_size = len % 8;
  if (trailing_block_size > 0) {
    uint8_t tmp = 0;
    switch (trailing_block_size) {
      case 7:
        if (x[pos++])
          tmp |= 0b0100'0000;
        [[fallthrough]];
      case 6:
        if (x[pos++])
          tmp |= 0b0010'0000;
        [[fallthrough]];
      case 5:
        if (x[pos++])
          tmp |= 0b0001'0000;
        [[fallthrough]];
      case 4:
        if (x[pos++])
          tmp |= 0b0000'1000;
        [[fallthrough]];
      case 3:
        if (x[pos++])
          tmp |= 0b0000'0100;
        [[fallthrough]];
      case 2:
        if (x[pos++])
          tmp |= 0b0000'0010;
        [[fallthrough]];
      case 1:
        if (x[pos++])
          tmp |= 0b0000'0001;
        [[fallthrough]];
      default:
        break;
    }
    if (!value(tmp)) {
      return false;
    }
  }
  return end_sequence();
}

bool binary_serializer_base::save_names(type_id_list xs) {
  return byte_writer::value(xs);
}

bool binary_serializer_base::save_ids(type_id_list xs) {
  if (!begin_sequence(xs.size()))
    return false;
  for (auto id : xs) {
    if (!value(detail::to_underlying(id)))
      return false;
  }
  return end_sequence();
}

bool binary_serializer_base::value(type_id_list xs) {
  return save_ids(xs);
}

bool binary_serializer_base::update(size_t offset, const_byte_span content) {
  auto buf = mut_bytes();
  if (offset > buf.size() || content.size() > buf.size() - offset) {
    set_error(make_error(sec::end_of_stream,
                         "cannot update buffer at given offset because it "
                         "would exceed the buffer size"));
    return false;
  }
  memcpy(buf.data() + offset, content.data(), content.size());
  return true;
}

const_byte_span binary_serializer::bytes() const {
  return buf_;
}

void binary_serializer::reset() {
  buf_.clear();
}

size_t binary_serializer::skip(size_t num_bytes) {
  return skip_bytes(buf_, num_bytes);
}

void binary_serializer::do_append(const std::byte* data, size_t size) {
  buf_.insert(buf_.end(), data, data + size);
}

byte_span binary_serializer::mut_bytes() {
  return buf_;
}

bool binary_serializer::value(const strong_actor_ptr& ptr) {
  if (context_) {
    detail::default_actor_handle_codec codec{*context_};
    return codec.save(*this, ptr);
  }
  emplace_error(sec::no_context);
  return false;
}

} // namespace caf
