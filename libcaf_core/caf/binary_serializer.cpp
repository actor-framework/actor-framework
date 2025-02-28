// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/binary_serializer.hpp"

#include "caf/actor_system.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/internal/fast_pimpl.hpp"

#include <iomanip>

namespace caf {

namespace {

template <class T>
inline constexpr auto max_value
  = static_cast<size_t>(std::numeric_limits<T>::max());

template <class T>
T compress_index(bool is_present, size_t value) {
  return is_present ? static_cast<T>(value) : T{-1};
}

class impl : public save_inspector_base<impl>,
             public internal::fast_pimpl<impl> {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector_base<binary_serializer>;

  using container_type = byte_buffer;

  using value_type = std::byte;

  // -- constructors, destructors, and assignment operators --------------------

  impl(byte_buffer& buf, actor_system* sys) noexcept
    : buf_(buf), write_pos_(buf.size()), context_(sys) {
    // nop
  }

  impl(const impl&) = delete;

  impl& operator=(const impl&) = delete;

  // -- properties -------------------------------------------------------------

  /// Returns the current execution unit.
  actor_system* context() const noexcept {
    return context_;
  }

  byte_buffer& buf() noexcept {
    return buf_;
  }

  const byte_buffer& buf() const noexcept {
    return buf_;
  }

  size_t write_pos() const noexcept {
    return write_pos_;
  }

  static constexpr bool has_human_readable_format() noexcept {
    return false;
  }

  // -- position management ----------------------------------------------------

  void seek(size_t offset) noexcept {
    write_pos_ = offset;
  }

  void skip(size_t num_bytes) {
    auto remaining = buf_.size() - write_pos_;
    if (remaining < num_bytes)
      buf_.insert(buf_.end(), num_bytes - remaining, std::byte{0});
    write_pos_ += num_bytes;
  }

  // -- interface functions ----------------------------------------------------

  void set_error(error stop_reason) override {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept override {
    return err_;
  }

  constexpr bool begin_object(type_id_t, std::string_view) noexcept {
    return true;
  }

  constexpr bool end_object() {
    return true;
  }

  constexpr bool begin_field(std::string_view) noexcept {
    return true;
  }

  bool begin_field(std::string_view, bool is_present) {
    auto val = static_cast<uint8_t>(is_present);
    return value(val);
  }

  bool begin_field(std::string_view, span<const type_id_t> types,
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

  bool begin_field(std::string_view, bool is_present,
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

  constexpr bool end_field() {
    return true;
  }

  constexpr bool begin_tuple(size_t) {
    return true;
  }

  constexpr bool end_tuple() {
    return true;
  }

  constexpr bool begin_key_value_pair() {
    return true;
  }

  constexpr bool end_key_value_pair() {
    return true;
  }

  bool begin_sequence(size_t list_size) {
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

  constexpr bool end_sequence() {
    return true;
  }

  bool begin_associative_array(size_t size) {
    return begin_sequence(size);
  }

  bool end_associative_array() {
    return end_sequence();
  }

  bool value(span<const std::byte> x) {
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
      // Ignore false positive for stringop-overread
      CAF_PUSH_STRINGOP_OVERREAD_WARNING
      buf_.insert(buf_.end(), mid, last);
      CAF_POP_WARNINGS
    }
    write_pos_ += x.size();
    CAF_ASSERT(write_pos_ <= buf_.size());
    return true;
  }

  bool value(std::byte x) {
    if (write_pos_ == buf_.size())
      buf_.emplace_back(x);
    else
      buf_[write_pos_] = x;
    ++write_pos_;
    return true;
  }

  bool value(bool x) {
    return value(static_cast<uint8_t>(x));
  }

  bool value(int8_t x) {
    return value(static_cast<std::byte>(x));
  }

  bool value(uint8_t x) {
    return value(static_cast<std::byte>(x));
  }

  bool value(int16_t x) {
    return int_value(x);
  }

  bool value(uint16_t x) {
    return int_value(x);
  }

  bool value(int32_t x) {
    return int_value(x);
  }

  bool value(uint32_t x) {
    return int_value(x);
  }

  bool value(int64_t x) {
    return int_value(x);
  }

  bool value(uint64_t x) {
    return int_value(x);
  }

  bool value(float x) {
    return int_value(detail::pack754(x));
  }

  bool value(double x) {
    return int_value(detail::pack754(x));
  }

  bool value(long double x) {
    // TODO: Our IEEE-754 conversion currently does not work for long double.
    // The
    //       standard does not guarantee a fixed representation for this type,
    //       but on X86 we can usually rely on 80-bit precision. For now, we
    //       fall back to string conversion.
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<long double>::digits) << x;
    auto tmp = oss.str();
    return value(tmp);
  }

  bool value(std::string_view x) {
    if (!begin_sequence(x.size()))
      return false;
    value(as_bytes(make_span(x)));
    return end_sequence();
  }

  bool value(const std::u16string& x) {
    auto str_size = x.size();
    if (!begin_sequence(str_size))
      return false;
    // The standard does not guarantee that char16_t is exactly 16 bits.
    for (auto c : x)
      int_value(static_cast<uint16_t>(c));
    return end_sequence();
  }

  bool value(const std::u32string& x) {
    auto str_size = x.size();
    if (!begin_sequence(str_size))
      return false;
    // The standard does not guarantee that char32_t is exactly 32 bits.
    for (auto c : x)
      int_value(static_cast<uint32_t>(c));
    return end_sequence();
  }

  bool value(const std::vector<bool>& x) {
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

  virtual bool value(const strong_actor_ptr& ptr) {
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

  virtual bool value(const weak_actor_ptr& ptr) {
    auto tmp = ptr.lock();
    return value(tmp);
  }

private:
  template <class T>
  bool int_value(T x) {
    using unsigned_type = detail::squashed_int_t<std::make_unsigned_t<T>>;
    auto y = detail::to_network_order(static_cast<unsigned_type>(x));
    return value(as_bytes(make_span(&y, 1)));
  }

  /// Stores the serialized output.
  byte_buffer& buf_;

  /// Stores the current offset for writing.
  size_t write_pos_ = 0;

  /// Provides access to the ::proxy_registry and to the ::actor_system.
  actor_system* context_ = nullptr;

  error err_;
};

} // namespace

// -- constructors, destructors, and assignment operators --------------------

binary_serializer::binary_serializer(byte_buffer& buf) noexcept {
  impl::construct(impl_, buf, nullptr);
}

binary_serializer::binary_serializer(actor_system& sys,
                                     byte_buffer& buf) noexcept {
  impl::construct(impl_, buf, &sys);
}

binary_serializer::~binary_serializer() {
  impl::destruct(impl_);
}

// -- properties -------------------------------------------------------------

actor_system* binary_serializer::context() const noexcept {
  return impl::cast(impl_).context();
}

byte_buffer& binary_serializer::buf() noexcept {
  return impl::cast(impl_).buf();
}

const byte_buffer& binary_serializer::buf() const noexcept {
  return impl::cast(impl_).buf();
}

size_t binary_serializer::write_pos() const noexcept {
  return impl::cast(impl_).write_pos();
}

// -- position management ----------------------------------------------------

void binary_serializer::seek(size_t offset) noexcept {
  return impl::cast(impl_).seek(offset);
}

void binary_serializer::skip(size_t num_bytes) {
  impl::cast(impl_).skip(num_bytes);
}

// -- interface functions ----------------------------------------------------

void binary_serializer::set_error(error stop_reason) {
  impl::cast(impl_).set_error(std::move(stop_reason));
}

error& binary_serializer::get_error() noexcept {
  return impl::cast(impl_).get_error();
}

bool binary_serializer::begin_object(type_id_t type_id,
                                     std::string_view type_name) noexcept {
  return impl::cast(impl_).begin_object(type_id, type_name);
}

bool binary_serializer::end_object() {
  return impl::cast(impl_).end_object();
}

bool binary_serializer::begin_field(std::string_view type_name) noexcept {
  return impl::cast(impl_).begin_field(type_name);
}

bool binary_serializer::begin_field(std::string_view type_name,
                                    bool is_present) {
  return impl::cast(impl_).begin_field(type_name, is_present);
}

bool binary_serializer::begin_field(std::string_view type_name,
                                    span<const type_id_t> types, size_t index) {
  return impl::cast(impl_).begin_field(type_name, types, index);
}

bool binary_serializer::begin_field(std::string_view type_name, bool is_present,
                                    span<const type_id_t> types, size_t index) {
  return impl::cast(impl_).begin_field(type_name, is_present, types, index);
}

bool binary_serializer::end_field() {
  return impl::cast(impl_).end_field();
}

bool binary_serializer::begin_tuple(size_t) {
  return impl::cast(impl_).end_field();
}

bool binary_serializer::end_tuple() {
  return impl::cast(impl_).end_field();
}

bool binary_serializer::begin_key_value_pair() {
  return impl::cast(impl_).begin_key_value_pair();
}

bool binary_serializer::end_key_value_pair() {
  return impl::cast(impl_).end_field();
}

bool binary_serializer::begin_sequence(size_t list_size) {
  return impl::cast(impl_).begin_sequence(list_size);
}

bool binary_serializer::end_sequence() {
  return impl::cast(impl_).end_sequence();
}

bool binary_serializer::begin_associative_array(size_t size) {
  return impl::cast(impl_).begin_associative_array(size);
}

bool binary_serializer::end_associative_array() {
  return impl::cast(impl_).end_associative_array();
}

bool binary_serializer::value(span<const std::byte> x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(std::byte x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(bool x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(int8_t x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(uint8_t x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(int16_t x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(uint16_t x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(int32_t x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(uint32_t x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(int64_t x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(uint64_t x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(float x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(double x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(long double x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(std::string_view x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(const std::u16string& x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(const std::u32string& x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(const std::vector<bool>& x) {
  return impl::cast(impl_).value(x);
}

bool binary_serializer::value(const strong_actor_ptr& ptr) {
  return impl::cast(impl_).value(std::move(ptr));
}

bool binary_serializer::value(const weak_actor_ptr& ptr) {
  return impl::cast(impl_).value(std::move(ptr));
}

} // namespace caf
