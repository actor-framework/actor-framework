// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/binary_deserializer.hpp"

#include "caf/actor_handle_codec.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/sec.hpp"

#include <sstream>
#include <type_traits>

namespace {

template <class T>
constexpr size_t max_value = static_cast<size_t>(std::numeric_limits<T>::max());

} // namespace

namespace caf {

class binary_deserializer_impl : public byte_reader {
public:
  // -- constructors, destructors, and assignment operators --------------------

  binary_deserializer_impl(const std::byte* buf, size_t size,
                           caf::actor_handle_codec* codec) noexcept
    : current_(buf), end_(buf + size), codec_(codec) {
    // nop
  }

  binary_deserializer_impl(const binary_deserializer_impl&) = delete;

  binary_deserializer_impl& operator=(const binary_deserializer_impl&) = delete;

  // -- properties -------------------------------------------------------------

  size_t remaining() const noexcept {
    return static_cast<size_t>(end_ - current_);
  }

  void skip(size_t num_bytes) {
    if (num_bytes > remaining())
      CAF_RAISE_ERROR("cannot skip past the end");
    current_ += num_bytes;
  }

  bool load_bytes(const_byte_span bytes) noexcept override {
    current_ = bytes.data();
    end_ = current_ + bytes.size();
    return true;
  }

  const std::byte* current() const noexcept {
    return current_;
  }

  const std::byte* end() const noexcept {
    return end_;
  }

  bool has_human_readable_format() const noexcept override {
    return false;
  }

  // -- overridden member functions --------------------------------------------

  void set_error(error stop_reason) override {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept override {
    return err_;
  }

  caf::actor_handle_codec* actor_handle_codec() noexcept override {
    return codec_;
  }

  bool fetch_next_object_type(type_id_t& type) noexcept override {
    type = invalid_type_id;
    emplace_error(sec::unsupported_operation,
                  "the default binary format does not embed type information");
    return false;
  }

  constexpr bool begin_object(type_id_t, std::string_view) noexcept override {
    return true;
  }

  constexpr bool end_object() noexcept override {
    return true;
  }

  constexpr bool begin_field(std::string_view) noexcept override {
    return true;
  }

  bool begin_field(std::string_view, bool& is_present) noexcept override {
    auto tmp = uint8_t{0};
    if (!value(tmp))
      return false;
    is_present = static_cast<bool>(tmp);
    return true;
  }

  bool begin_field(std::string_view, std::span<const type_id_t> types,
                   size_t& index) noexcept override {
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

  bool begin_field(std::string_view, bool& is_present,
                   std::span<const type_id_t> types,
                   size_t& index) noexcept override {
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
  constexpr bool end_field() override {
    return true;
  }

  constexpr bool begin_tuple(size_t) noexcept override {
    return true;
  }

  constexpr bool end_tuple() noexcept override {
    return true;
  }

  constexpr bool begin_key_value_pair() noexcept override {
    return true;
  }

  constexpr bool end_key_value_pair() noexcept override {
    return true;
  }

  bool begin_sequence(size_t& list_size) noexcept override {
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

  constexpr bool end_sequence() noexcept override {
    return true;
  }

  bool begin_associative_array(size_t& size) noexcept override {
    return begin_sequence(size);
  }

  bool end_associative_array() noexcept override {
    return end_sequence();
  }

  bool value(bool& x) noexcept override {
    int8_t tmp = 0;
    if (!value(tmp))
      return false;
    x = tmp != 0;
    return true;
  }

  bool value(std::byte& x) noexcept override {
    if (range_check(1)) {
      x = *current_++;
      return true;
    }
    emplace_error(sec::end_of_stream);
    return false;
  }

  bool value(int8_t& x) noexcept override {
    if (range_check(1)) {
      x = static_cast<int8_t>(*current_++);
      return true;
    }
    emplace_error(sec::end_of_stream);
    return false;
  }

  bool value(uint8_t& x) noexcept override {
    if (range_check(1)) {
      x = static_cast<uint8_t>(*current_++);
      return true;
    }
    emplace_error(sec::end_of_stream);
    return false;
  }

  bool value(int16_t& x) noexcept override {
    return int_value(x);
  }

  bool value(uint16_t& x) noexcept override {
    return int_value(x);
  }

  bool value(int32_t& x) noexcept override {
    return int_value(x);
  }

  bool value(uint32_t& x) noexcept override {
    return int_value(x);
  }

  bool value(int64_t& x) noexcept override {
    return int_value(x);
  }

  bool value(uint64_t& x) noexcept override {
    return int_value(x);
  }

  bool value(float& x) noexcept override {
    return float_value(x);
  }

  bool value(double& x) noexcept override {
    return float_value(x);
  }

  bool value(long double& x) override {
    // TODO: Our IEEE-754 conversion currently does not work for long double.
    //       The standard does not guarantee a fixed representation for this
    //       type, but on X86 we can usually rely on 80-bit precision. For now,
    //       we fall back to string conversion.
    std::string tmp;
    if (!value(tmp))
      return false;
    std::istringstream iss{std::move(tmp)};
    if (iss >> x)
      return true;
    emplace_error(sec::invalid_argument);
    return false;
  }

  bool value(byte_span x) noexcept override {
    if (!range_check(x.size())) {
      emplace_error(sec::end_of_stream);
      return false;
    }
    memcpy(x.data(), current_, x.size());
    current_ += x.size();
    return true;
  }

  bool value(std::string& x) override {
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

  bool value(std::u16string& x) override {
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
      unsafe_int_value(tmp);
      x.push_back(static_cast<char16_t>(tmp));
    }
    return end_sequence();
  }

  bool value(std::u32string& x) override {
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
      unsafe_int_value(tmp);
      x.push_back(static_cast<char32_t>(tmp));
    }
    return end_sequence();
  }

  bool value(std::vector<bool>& what) override {
    what.clear();
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
      what.emplace_back((tmp & 0b1000'0000) != 0);
      what.emplace_back((tmp & 0b0100'0000) != 0);
      what.emplace_back((tmp & 0b0010'0000) != 0);
      what.emplace_back((tmp & 0b0001'0000) != 0);
      what.emplace_back((tmp & 0b0000'1000) != 0);
      what.emplace_back((tmp & 0b0000'0100) != 0);
      what.emplace_back((tmp & 0b0000'0010) != 0);
      what.emplace_back((tmp & 0b0000'0001) != 0);
    }
    auto trailing_block_size = len % 8;
    if (trailing_block_size > 0) {
      uint8_t tmp = 0;
      if (!value(tmp))
        return false;
      switch (trailing_block_size) {
        case 7:
          what.emplace_back((tmp & 0b0100'0000) != 0);
          [[fallthrough]];
        case 6:
          what.emplace_back((tmp & 0b0010'0000) != 0);
          [[fallthrough]];
        case 5:
          what.emplace_back((tmp & 0b0001'0000) != 0);
          [[fallthrough]];
        case 4:
          what.emplace_back((tmp & 0b0000'1000) != 0);
          [[fallthrough]];
        case 3:
          what.emplace_back((tmp & 0b0000'0100) != 0);
          [[fallthrough]];
        case 2:
          what.emplace_back((tmp & 0b0000'0010) != 0);
          [[fallthrough]];
        case 1:
          what.emplace_back((tmp & 0b0000'0001) != 0);
          [[fallthrough]];
        default:
          break;
      }
    }
    return end_sequence();
  }

private:
  /// Checks whether we can read `read_size` more bytes.
  bool range_check(size_t read_size) const noexcept {
    return current_ + read_size <= end_;
  }

  template <class T>
  bool int_value(T& x) {
    auto tmp = std::make_unsigned_t<T>{};
    if (value(as_writable_bytes(std::span{&tmp, 1}))) {
      x = static_cast<T>(detail::from_network_order(tmp));
      return true;
    } else {
      return false;
    }
  }

  template <class T>
  bool float_value(T& x) {
    auto tmp = typename detail::ieee_754_trait<T>::packed_type{};
    if (int_value(tmp)) {
      x = detail::unpack754(tmp);
      return true;
    } else {
      return false;
    }
  }

  // Does not perform any range checks.
  template <class T>
  void unsafe_int_value(T& x) {
    std::make_unsigned_t<T> tmp;
    memcpy(&tmp, current(), sizeof(tmp));
    skip(sizeof(tmp));
    x = static_cast<T>(detail::from_network_order(tmp));
  }

  /// Points to the current read position.
  const std::byte* current_;

  /// Points to the end of the assigned memory block.
  const std::byte* end_;

  caf::actor_handle_codec* codec_ = nullptr;

  /// The last occurred error.
  error err_;
};

// -- constructors, destructors, and assignment operators --------------------

binary_deserializer::binary_deserializer(
  const_byte_span input, caf::actor_handle_codec* codec) noexcept
  : super(new(impl_storage_)
            binary_deserializer_impl(input.data(), input.size(), codec)) {
  static_assert(sizeof(binary_deserializer_impl) <= impl_storage_size);
}

binary_deserializer::binary_deserializer(
  const void* buf, size_t size, caf::actor_handle_codec* codec) noexcept
  : super(new(impl_storage_) binary_deserializer_impl(
      reinterpret_cast<const std::byte*>(buf), size, codec)) {
  // nop
}

binary_deserializer::~binary_deserializer() noexcept {
  // nop
}

} // namespace caf
