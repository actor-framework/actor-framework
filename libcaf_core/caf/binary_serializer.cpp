// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/binary_serializer.hpp"

#include "caf/actor_handle_codec.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/sec.hpp"
#include "caf/serializer.hpp"

#include <iomanip>
#include <span>

namespace {

template <class T>
inline constexpr auto max_value
  = static_cast<size_t>(std::numeric_limits<T>::max());

template <class T>
T compress_index(bool is_present, size_t value) {
  return is_present ? static_cast<T>(value) : T{-1};
}

} // namespace

namespace caf {

class binary_serializer_impl : public byte_writer {
public:
  // -- member types -----------------------------------------------------------

  // -- constructors, destructors, and assignment operators --------------------

  explicit binary_serializer_impl(byte_buffer& buf,
                                  caf::actor_handle_codec* codec) noexcept
    : buf_(buf), write_pos_(buf.size()), codec_(codec) {
    // nop
  }

  binary_serializer_impl(const binary_serializer_impl&) = delete;

  binary_serializer_impl& operator=(const binary_serializer_impl&) = delete;

  // -- properties -------------------------------------------------------------

  const_byte_span bytes() const noexcept override {
    return buf_;
  }

  bool has_human_readable_format() const noexcept override {
    return false;
  }

  void reset() noexcept override {
    buf_.clear();
    write_pos_ = 0;
  }

  // -- position management ----------------------------------------------------

  size_t skip(size_t num_bytes) override {
    auto offset = write_pos_;
    auto remaining = buf_.size() - write_pos_;
    if (remaining < num_bytes)
      buf_.insert(buf_.end(), num_bytes - remaining, std::byte{0});
    write_pos_ += num_bytes;
    return offset;
  }

  bool update(size_t offset, const_byte_span content) noexcept override {
    if (offset + content.size() > buf_.size()) {
      set_error(make_error(sec::end_of_stream,
                           "cannot update buffer at given offset because it "
                           "would exceed the buffer size"));
      return false;
    }
    memcpy(buf_.data() + offset, content.data(), content.size());
    return true;
  }

  // -- interface functions ----------------------------------------------------

  void set_error(error stop_reason) override {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept override {
    return err_;
  }

  caf::actor_handle_codec* actor_handle_codec() noexcept override {
    return codec_;
  }

  constexpr bool begin_object(type_id_t, std::string_view) noexcept override {
    return true;
  }

  constexpr bool end_object() override {
    return true;
  }

  constexpr bool begin_field(std::string_view) noexcept override {
    return true;
  }

  bool begin_field(std::string_view, bool is_present) override {
    auto val = static_cast<uint8_t>(is_present);
    return value(val);
  }

  bool begin_field(std::string_view, std::span<const type_id_t> types,
                   size_t index) override {
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

  bool begin_field(std::string_view, bool is_present,
                   std::span<const type_id_t> types, size_t index) override {
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

  constexpr bool end_field() override {
    return true;
  }

  constexpr bool begin_tuple(size_t) override {
    return true;
  }

  constexpr bool end_tuple() override {
    return true;
  }

  constexpr bool begin_key_value_pair() override {
    return true;
  }

  constexpr bool end_key_value_pair() override {
    return true;
  }

  bool begin_sequence(size_t list_size) override {
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

  constexpr bool end_sequence() override {
    return true;
  }

  bool begin_associative_array(size_t size) override {
    return begin_sequence(size);
  }

  bool end_associative_array() override {
    return end_sequence();
  }

  bool value(const_byte_span x) override {
    CAF_ASSERT(write_pos_ <= buf_.size());
    auto buf_size = buf_.size();
    if (write_pos_ == buf_size) {
      buf_.insert(buf_.end(), x.begin(), x.end());
    } else if (write_pos_ + x.size() <= buf_size) {
      memcpy(buf_.data() + write_pos_, x.data(), x.size());
    } else {
      auto remaining = buf_size - write_pos_;
      CAF_ASSERT(remaining < x.size());
      auto first = x.data();
      auto mid = first + remaining;
      auto last = first + x.size();
      memcpy(buf_.data() + write_pos_, first, remaining);
      // Ignore false positive for stringop-overread
      CAF_PUSH_STRINGOP_OVERREAD_WARNING
      buf_.insert(buf_.end(), mid, last);
      CAF_POP_WARNINGS
    }
    write_pos_ += x.size();
    CAF_ASSERT(write_pos_ <= buf_.size());
    return true;
  }

  bool value(std::byte x) override {
    if (write_pos_ == buf_.size())
      buf_.emplace_back(x);
    else
      buf_[write_pos_] = x;
    ++write_pos_;
    return true;
  }

  bool value(bool x) override {
    return value(static_cast<uint8_t>(x));
  }

  bool value(int8_t x) override {
    return value(static_cast<std::byte>(x));
  }

  bool value(uint8_t x) override {
    return value(static_cast<std::byte>(x));
  }

  bool value(int16_t x) override {
    return int_value(x);
  }

  bool value(uint16_t x) override {
    return int_value(x);
  }

  bool value(int32_t x) override {
    return int_value(x);
  }

  bool value(uint32_t x) override {
    return int_value(x);
  }

  bool value(int64_t x) override {
    return int_value(x);
  }

  bool value(uint64_t x) override {
    return int_value(x);
  }

  bool value(float x) override {
    return int_value(detail::pack754(x));
  }

  bool value(double x) override {
    return int_value(detail::pack754(x));
  }

  bool value(long double x) override {
    // TODO: Our IEEE-754 conversion currently does not work for long double.
    //       The standard does not guarantee a fixed representation for this
    //       type, but on X86 we can usually rely on 80-bit precision. For now,
    //       we fall back to string conversion.
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
    auto tmp = oss.str();
    return value(tmp);
  }

  bool value(std::string_view x) override {
    if (!begin_sequence(x.size()))
      return false;
    value(as_bytes(std::span{x}));
    return end_sequence();
  }

  bool value(const std::u16string& x) override {
    auto str_size = x.size();
    if (!begin_sequence(str_size))
      return false;
    // The standard does not guarantee that char16_t is exactly 16 bits.
    for (auto c : x)
      int_value(static_cast<uint16_t>(c));
    return end_sequence();
  }

  bool value(const std::u32string& x) override {
    auto str_size = x.size();
    if (!begin_sequence(str_size))
      return false;
    // The standard does not guarantee that char32_t is exactly 32 bits.
    for (auto c : x)
      int_value(static_cast<uint32_t>(c));
    return end_sequence();
  }

  bool value(const std::vector<bool>& what) override {
    auto len = what.size();
    if (!begin_sequence(len))
      return false;
    if (len == 0)
      return end_sequence();
    size_t pos = 0;
    size_t blocks = len / 8;
    for (size_t block = 0; block < blocks; ++block) {
      uint8_t tmp = 0;
      if (what[pos++])
        tmp |= 0b1000'0000;
      if (what[pos++])
        tmp |= 0b0100'0000;
      if (what[pos++])
        tmp |= 0b0010'0000;
      if (what[pos++])
        tmp |= 0b0001'0000;
      if (what[pos++])
        tmp |= 0b0000'1000;
      if (what[pos++])
        tmp |= 0b0000'0100;
      if (what[pos++])
        tmp |= 0b0000'0010;
      if (what[pos++])
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
          if (what[pos++])
            tmp |= 0b0100'0000;
          [[fallthrough]];
        case 6:
          if (what[pos++])
            tmp |= 0b0010'0000;
          [[fallthrough]];
        case 5:
          if (what[pos++])
            tmp |= 0b0001'0000;
          [[fallthrough]];
        case 4:
          if (what[pos++])
            tmp |= 0b0000'1000;
          [[fallthrough]];
        case 3:
          if (what[pos++])
            tmp |= 0b0000'0100;
          [[fallthrough]];
        case 2:
          if (what[pos++])
            tmp |= 0b0000'0010;
          [[fallthrough]];
        case 1:
          if (what[pos++])
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

private:
  template <class T>
  bool int_value(T x) {
    using unsigned_type = detail::squashed_int_t<std::make_unsigned_t<T>>;
    auto y = detail::to_network_order(static_cast<unsigned_type>(x));
    return value(as_bytes(std::span{&y, 1}));
  }

  /// Stores the serialized output.
  byte_buffer& buf_;

  /// Stores the current offset for writing.
  size_t write_pos_ = 0;

  caf::actor_handle_codec* codec_ = nullptr;

  error err_;
};

// -- constructors, destructors, and assignment operators --------------------

binary_serializer::binary_serializer(byte_buffer& buf,
                                     caf::actor_handle_codec* codec) noexcept
  : super(new(impl_storage_) binary_serializer_impl(buf, codec)) {
  static_assert(sizeof(binary_serializer_impl) <= impl_storage_size);
}

binary_serializer::~binary_serializer() noexcept {
  // nop
}

} // namespace caf
