// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/binary_serializer.hpp"

#include "caf/actor_system.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/squashed_int.hpp"

#include <iomanip>

namespace caf {

namespace {

template <class T>
auto int_value(binary_serializer& sink, T x) {
  using unsigned_type = detail::squashed_int_t<std::make_unsigned_t<T>>;
  auto y = detail::to_network_order(static_cast<unsigned_type>(x));
  return sink.value(as_bytes(make_span(&y, 1)));
}

} // namespace

void binary_serializer::skip(size_t num_bytes) {
  auto remaining = buf_.size() - write_pos_;
  if (remaining < num_bytes)
    buf_.insert(buf_.end(), num_bytes - remaining, std::byte{0});
  write_pos_ += num_bytes;
}

bool binary_serializer::begin_sequence(size_t list_size) {
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
  return value(as_bytes(make_span(buf, static_cast<size_t>(i - buf))));
}

bool binary_serializer::begin_field(std::string_view, bool is_present) {
  auto val = static_cast<uint8_t>(is_present);
  return value(val);
}

namespace {

template <class T>
constexpr size_t max_value = static_cast<size_t>(std::numeric_limits<T>::max());

} // namespace

bool binary_serializer::begin_field(std::string_view,
                                    span<const type_id_t> types, size_t index) {
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

namespace {

template <class T>
T compress_index(bool is_present, size_t value) {
  // return is_present ? static_cast<T>(value) : T{-1};
  auto val = is_present ? static_cast<T>(value) : T{-1};
  return val;
}

} // namespace

bool binary_serializer::begin_field(std::string_view, bool is_present,
                                    span<const type_id_t> types, size_t index) {
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

bool binary_serializer::value(span<const std::byte> x) {
  CAF_ASSERT(write_pos_ <= buf_.size());
  auto buf_size = buf_.size();
  if (write_pos_ == buf_size) {
    buf_.insert(buf_.end(), x.begin(), x.end());
  } else if (write_pos_ + x.size() <= buf_size) {
    memcpy(buf_.data() + write_pos_, x.data(), x.size());
  } else {
    auto remaining = buf_size - write_pos_;
    CAF_ASSERT(remaining < x.size());
    auto first = x.begin();
    auto mid = first + remaining;
    auto last = x.end();
    memcpy(buf_.data() + write_pos_, first, remaining);
    buf_.insert(buf_.end(), mid, last);
  }
  write_pos_ += x.size();
  CAF_ASSERT(write_pos_ <= buf_.size());
  return true;
}

bool binary_serializer::value(std::byte x) {
  if (write_pos_ == buf_.size())
    buf_.emplace_back(x);
  else
    buf_[write_pos_] = x;
  ++write_pos_;
  return true;
}

bool binary_serializer::value(bool x) {
  return value(static_cast<uint8_t>(x));
}

bool binary_serializer::value(int8_t x) {
  return value(static_cast<std::byte>(x));
}

bool binary_serializer::value(uint8_t x) {
  return value(static_cast<std::byte>(x));
}

bool binary_serializer::value(int16_t x) {
  return int_value(*this, x);
}

bool binary_serializer::value(uint16_t x) {
  return int_value(*this, x);
}

bool binary_serializer::value(int32_t x) {
  return int_value(*this, x);
}

bool binary_serializer::value(uint32_t x) {
  return int_value(*this, x);
}

bool binary_serializer::value(int64_t x) {
  return int_value(*this, x);
}

bool binary_serializer::value(uint64_t x) {
  return int_value(*this, x);
}

bool binary_serializer::value(float x) {
  return int_value(*this, detail::pack754(x));
}

bool binary_serializer::value(double x) {
  return int_value(*this, detail::pack754(x));
}

bool binary_serializer::value(long double x) {
  // TODO: Our IEEE-754 conversion currently does not work for long double. The
  //       standard does not guarantee a fixed representation for this type, but
  //       on X86 we can usually rely on 80-bit precision. For now, we fall back
  //       to string conversion.
  std::ostringstream oss;
  oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
  auto tmp = oss.str();
  return value(tmp);
}

bool binary_serializer::value(std::string_view x) {
  if (!begin_sequence(x.size()))
    return false;
  value(as_bytes(make_span(x)));
  return end_sequence();
}

bool binary_serializer::value(const std::u16string& x) {
  auto str_size = x.size();
  if (!begin_sequence(str_size))
    return false;
  // The standard does not guarantee that char16_t is exactly 16 bits.
  for (auto c : x)
    int_value(*this, static_cast<uint16_t>(c));
  return end_sequence();
}

bool binary_serializer::value(const std::u32string& x) {
  auto str_size = x.size();
  if (!begin_sequence(str_size))
    return false;
  // The standard does not guarantee that char32_t is exactly 32 bits.
  for (auto c : x)
    int_value(*this, static_cast<uint32_t>(c));
  return end_sequence();
}

bool binary_serializer::value(const std::vector<bool>& x) {
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
    value(tmp);
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
    value(tmp);
  }
  return end_sequence();
}

bool binary_serializer::value(const strong_actor_ptr& ptr) {
  actor_id aid = 0;
  node_id nid;
  if (ptr != nullptr) {
    aid = ptr->id();
    nid = ptr->node();
  }
  if (!value(aid)) {
    return false;
  }
  if (!inspect(*this, nid)) {
    return false;
  }
  if (ptr != nullptr) {
    if (auto err = save_actor(ptr, aid, nid)) {
      set_error(err);
      return false;
    }
  }
  return true;
}

bool binary_serializer::value(const weak_actor_ptr& ptr) {
  auto tmp = ptr.lock();
  return value(tmp);
}

} // namespace caf
