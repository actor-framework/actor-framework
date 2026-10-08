// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/binary_deserializer.hpp"

#include "caf/actor_handle_codec.hpp"
#include "caf/detail/default_actor_handle_codec.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/type_id_list_builder.hpp"
#include "caf/error.hpp"
#include "caf/raise_error.hpp"
#include "caf/sec.hpp"
#include "caf/type_id.hpp"
#include "caf/type_id_list.hpp"

#include <cstring>
#include <limits>
#include <sstream>
#include <type_traits>

namespace {

template <class T>
constexpr size_t max_value = static_cast<size_t>(std::numeric_limits<T>::max());

} // namespace

namespace caf {

size_t binary_deserializer_base::remaining() const noexcept {
  return static_cast<size_t>(end_ - pos_);
}

void binary_deserializer_base::skip(size_t num_bytes) {
  if (num_bytes > remaining())
    CAF_RAISE_ERROR("cannot skip past the end");
  pos_ += num_bytes;
}

bool binary_deserializer_base::range_check(size_t read_size) const noexcept {
  return read_size <= remaining();
}

template <class T>
bool binary_deserializer_base::int_value(T& x) {
  auto tmp = std::make_unsigned_t<T>{};
  if (value(as_writable_bytes(std::span{&tmp, 1}))) {
    x = static_cast<T>(detail::from_network_order(tmp));
    return true;
  }
  return false;
}

template <class T>
bool binary_deserializer_base::float_value(T& x) {
  auto tmp = typename detail::ieee_754_trait<T>::packed_type{};
  if (int_value(tmp)) {
    x = detail::unpack754(tmp);
    return true;
  }
  return false;
}

template <class T>
void binary_deserializer_base::unsafe_int_value(T& x) {
  std::make_unsigned_t<T> tmp;
  memcpy(&tmp, pos_, sizeof(tmp));
  skip(sizeof(tmp));
  x = static_cast<T>(detail::from_network_order(tmp));
}

bool binary_deserializer_base::load_bytes(const_byte_span bytes) noexcept {
  pos_ = bytes.data();
  end_ = pos_ + bytes.size();
  return true;
}

bool binary_deserializer_base::has_human_readable_format() const noexcept {
  return false;
}

void binary_deserializer_base::set_error(error stop_reason) {
  err_ = std::move(stop_reason);
}

error& binary_deserializer_base::get_error() noexcept {
  return err_;
}

bool binary_deserializer_base::fetch_next_object_type(
  type_id_t& type) noexcept {
  type = invalid_type_id;
  emplace_error(sec::unsupported_operation,
                "the default binary format does not embed type "
                "information");
  return false;
}

bool binary_deserializer_base::begin_object(type_id_t,
                                            std::string_view) noexcept {
  return true;
}

bool binary_deserializer_base::end_object() noexcept {
  return true;
}

bool binary_deserializer_base::begin_field(std::string_view) noexcept {
  return true;
}

bool binary_deserializer_base::begin_field(std::string_view,
                                           bool& is_present) noexcept {
  auto tmp = uint8_t{0};
  if (!value(tmp))
    return false;
  is_present = static_cast<bool>(tmp);
  return true;
}

bool binary_deserializer_base::begin_field(std::string_view,
                                           std::span<const type_id_t> types,
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

bool binary_deserializer_base::begin_field(std::string_view, bool& is_present,
                                           std::span<const type_id_t> types,
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

bool binary_deserializer_base::end_field() {
  return true;
}

bool binary_deserializer_base::begin_tuple(size_t) noexcept {
  return true;
}

bool binary_deserializer_base::end_tuple() noexcept {
  return true;
}

bool binary_deserializer_base::begin_key_value_pair() noexcept {
  return true;
}

bool binary_deserializer_base::end_key_value_pair() noexcept {
  return true;
}

bool binary_deserializer_base::begin_sequence(size_t& list_size) noexcept {
  // A 32-bit length uses at most five varbyte chunks. Four chunks carry seven
  // bits each; the fifth chunk carries the remaining four bits.
  uint32_t x = 0;
  for (int n = 0; n < 4; ++n) {
    auto low7 = uint8_t{0};
    if (!value(low7)) {
      return false;
    }
    x |= static_cast<uint32_t>(low7 & 0x7F) << (7 * n);
    if ((low7 & 0x80) == 0) {
      list_size = x;
      return true;
    }
  }
  auto low7 = uint8_t{0};
  if (!value(low7)) {
    return false;
  }
  // The high nibble is the continuation bit plus three bits past bit 31.
  if ((low7 & 0xF0) != 0) {
    emplace_error(sec::invalid_argument);
    return false;
  }
  x |= static_cast<uint32_t>(low7) << 28;
  list_size = x;
  return true;
}

bool binary_deserializer_base::end_sequence() noexcept {
  return true;
}

bool binary_deserializer_base::begin_associative_array(size_t& size) noexcept {
  return begin_sequence(size);
}

bool binary_deserializer_base::end_associative_array() noexcept {
  return end_sequence();
}

bool binary_deserializer_base::value(bool& x) noexcept {
  int8_t tmp = 0;
  if (!value(tmp))
    return false;
  x = tmp != 0;
  return true;
}

bool binary_deserializer_base::value(std::byte& x) noexcept {
  if (range_check(1)) {
    x = *pos_++;
    return true;
  }
  emplace_error(sec::end_of_stream);
  return false;
}

bool binary_deserializer_base::value(int8_t& x) noexcept {
  if (range_check(1)) {
    x = static_cast<int8_t>(*pos_++);
    return true;
  }
  emplace_error(sec::end_of_stream);
  return false;
}

bool binary_deserializer_base::value(uint8_t& x) noexcept {
  if (range_check(1)) {
    x = static_cast<uint8_t>(*pos_++);
    return true;
  }
  emplace_error(sec::end_of_stream);
  return false;
}

bool binary_deserializer_base::value(int16_t& x) noexcept {
  return int_value(x);
}

bool binary_deserializer_base::value(uint16_t& x) noexcept {
  return int_value(x);
}

bool binary_deserializer_base::value(int32_t& x) noexcept {
  return int_value(x);
}

bool binary_deserializer_base::value(uint32_t& x) noexcept {
  return int_value(x);
}

bool binary_deserializer_base::value(int64_t& x) noexcept {
  return int_value(x);
}

bool binary_deserializer_base::value(uint64_t& x) noexcept {
  return int_value(x);
}

bool binary_deserializer_base::value(float& x) noexcept {
  return float_value(x);
}

bool binary_deserializer_base::value(double& x) noexcept {
  return float_value(x);
}

bool binary_deserializer_base::value(long double& x) {
  std::string tmp;
  if (!value(tmp))
    return false;
  std::istringstream iss{std::move(tmp)};
  if (iss >> x)
    return true;
  emplace_error(sec::invalid_argument);
  return false;
}

bool binary_deserializer_base::value(byte_span x) noexcept {
  if (!range_check(x.size())) {
    emplace_error(sec::end_of_stream);
    return false;
  }
  memcpy(x.data(), pos_, x.size());
  pos_ += x.size();
  return true;
}

bool binary_deserializer_base::value(std::string& x) {
  x.clear();
  size_t str_size = 0;
  if (!begin_sequence(str_size))
    return false;
  if (!range_check(str_size)) {
    emplace_error(sec::end_of_stream);
    return false;
  }
  x.assign(reinterpret_cast<const char*>(pos_), str_size);
  pos_ += str_size;
  return end_sequence();
}

bool binary_deserializer_base::value(std::u16string& x) {
  x.clear();
  size_t str_size = 0;
  if (!begin_sequence(str_size))
    return false;
  if (!range_check(str_size * sizeof(uint16_t))) {
    emplace_error(sec::end_of_stream);
    return false;
  }
  for (size_t i = 0; i < str_size; ++i) {
    uint16_t tmp;
    unsafe_int_value(tmp);
    x.push_back(static_cast<char16_t>(tmp));
  }
  return end_sequence();
}

bool binary_deserializer_base::value(std::u32string& x) {
  x.clear();
  size_t str_size = 0;
  if (!begin_sequence(str_size))
    return false;
  if (!range_check(str_size * sizeof(uint32_t))) {
    emplace_error(sec::end_of_stream);
    return false;
  }
  for (size_t i = 0; i < str_size; ++i) {
    uint32_t tmp;
    unsafe_int_value(tmp);
    x.push_back(static_cast<char32_t>(tmp));
  }
  return end_sequence();
}

bool binary_deserializer_base::value(std::vector<bool>& x) {
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

bool binary_deserializer_base::value(type_id_list& xs) {
  size_t size = 0;
  if (!begin_sequence(size))
    return false;
  if (size > type_id_list::max_size) {
    emplace_error(sec::invalid_argument);
    return false;
  }
  if (size == 0) {
    xs = make_type_id_list();
    return end_sequence();
  }
  detail::type_id_list_builder ids{size};
  using type_id_int_t = std::underlying_type_t<type_id_t>;
  for (size_t i = 0; i < size; ++i) {
    auto id = type_id_int_t{0};
    if (!value(id))
      return false;
    ids.push_back(static_cast<type_id_t>(id));
  }
  if (!end_sequence())
    return false;
  xs = ids.move_to_list();
  return true;
}

type_id_t binary_deserializer::to_type_id(std::string_view name) const {
  default_type_id_mapper mapper;
  return mapper(name);
}

bool binary_deserializer::value(strong_actor_ptr& ptr) {
  if (context_) {
    detail::default_actor_handle_codec codec{*context_};
    return codec.load(*this, ptr);
  }
  emplace_error(sec::no_context);
  return false;
}

type_id_t binary_deserializer_v2::to_type_id(std::string_view name) const {
  return (*mapper_)(name);
}

bool binary_deserializer_v2::value(type_id_list& xs) {
  if (use_type_names_)
    return byte_reader::value(xs);
  return super::value(xs);
}

bool binary_deserializer_v2::value(strong_actor_ptr& ptr) {
  if (codec_)
    return codec_->load(*this, ptr);
  emplace_error(sec::no_actor_handle_codec);
  return false;
}

} // namespace caf
