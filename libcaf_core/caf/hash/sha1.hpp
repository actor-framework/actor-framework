// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace caf::hash {

/// US Secure Hash Algorithm 1 (SHA1) as defined in RFC 3174.
class CAF_CORE_EXPORT sha1 : public save_inspector_base<sha1> {
public:
  /// Hash size in bytes.
  static constexpr size_t hash_size = 20;

  /// Alias to the super types.
  using super = save_inspector_base<sha1>;

  /// Array type for storing a 160-bit hash.
  using result_type = std::array<std::byte, hash_size>;

  sha1() noexcept;

  static constexpr bool has_human_readable_format() noexcept {
    return false;
  }

  constexpr bool begin_object(type_id_t, std::string_view) {
    return true;
  }

  constexpr bool end_object() {
    return true;
  }

  bool begin_field(std::string_view) {
    return true;
  }

  bool begin_field(std::string_view, bool is_present) {
    return value(static_cast<uint8_t>(is_present));
  }

  bool begin_field(std::string_view, span<const type_id_t>, size_t index) {
    return value(index);
  }

  bool begin_field(std::string_view, bool is_present, span<const type_id_t>,
                   size_t index) {
    value(static_cast<uint8_t>(is_present));
    if (is_present)
      value(index);
    return true;
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

  constexpr bool begin_sequence(size_t) {
    return true;
  }

  constexpr bool end_sequence() {
    return true;
  }

  constexpr bool begin_associative_array(size_t) {
    return true;
  }

  constexpr bool end_associative_array() {
    return true;
  }

  template <class Integral>
  std::enable_if_t<std::is_integral<Integral>::value, bool>
  value(Integral x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(&x);
    append(begin, begin + sizeof(Integral));
    return true;
  }

  bool value(bool x) noexcept {
    auto tmp = static_cast<uint8_t>(x);
    return value(tmp);
  }

  bool value(float x) noexcept {
    return value(detail::pack754(x));
  }

  bool value(double x) noexcept {
    return value(detail::pack754(x));
  }

  bool value(std::string_view x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(x.data());
    append(begin, begin + x.size());
    return true;
  }

  bool value(span<const std::byte> x) noexcept {
    auto begin = reinterpret_cast<const uint8_t*>(x.data());
    append(begin, begin + x.size());
    return true;
  }

  /// Seals this SHA-1 context and returns the 160-bit message digest.
  result_type result() noexcept;

  /// Convenience function for computing a SHA-1 hash value for given arguments
  /// in one shot.
  template <class... Ts>
  static result_type compute(Ts&&... xs) noexcept {
    using detail::as_mutable_ref;
    sha1 f;
    auto unused = f.apply(xs...);
    static_cast<void>(unused); // Always true.
    return f.result();
  }

private:
  bool append(const uint8_t* begin, const uint8_t* end) noexcept;

  void process_message_block();

  void pad_message();

  /// Stores whether `result()` has been called.
  bool sealed_ = 0;

  /// Stores the message digest so far.
  std::array<uint32_t, hash_size / 4> intermediate_;

  /// Stores the message length in bits.
  uint64_t length_ = 0;

  /// Stores the current index in `message_block_`.
  int_least16_t message_block_index_ = 0;

  /// Stores 512-bit message blocks.
  std::array<uint8_t, 64> message_block_;
};

} // namespace caf::hash
