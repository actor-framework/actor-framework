// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/uuid.hpp"

#include "caf/detail/append_hex.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/expected.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/message.hpp"
#include "caf/parser_state.hpp"

#include <cctype>
#include <cstring>
#include <random>

namespace caf {

namespace {

std::byte nil_bytes[16];

} // namespace

uuid::uuid() noexcept {
  memset(bytes_.data(), 0, bytes_.size());
}

uuid::operator bool() const noexcept {
  return memcmp(bytes_.data(), nil_bytes, 16) != 0;
}

bool uuid::operator!() const noexcept {
  return memcmp(bytes_.data(), nil_bytes, 16) == 0;
}

namespace {

// The following table lists the contents of the variant field, where the
// letter "x" indicates a "don't-care" value.
//
// ```
//   Msb0  Msb1  Msb2  Description
//
//    0     x     x    Reserved, NCS backward compatibility.
//
//    1     0     x    The variant specified in this document.
//
//    1     1     0    Reserved, Microsoft Corporation backward
//                     compatibility
//
//    1     1     1    Reserved for future definition.
// ```
uuid::variant_field variant_table[] = {
  uuid::reserved,  // 0 0 0
  uuid::reserved,  // 0 0 1
  uuid::reserved,  // 0 1 0
  uuid::reserved,  // 0 1 1
  uuid::rfc4122,   // 1 0 0
  uuid::rfc4122,   // 1 0 1
  uuid::microsoft, // 1 1 0
  uuid::reserved,  // 1 1 1
};

} // namespace

uuid::variant_field uuid::variant() const noexcept {
  return variant_table[std::to_integer<size_t>(bytes_[8]) >> 5];
}

uuid::version_field uuid::version() const noexcept {
  return static_cast<version_field>(std::to_integer<int>(bytes_[6]) >> 4);
}

uint64_t uuid::timestamp() const noexcept {
  // Assemble octets like this (L = low, M = mid, H = high):
  // 0H HH MM MM LL LL LL LL
  std::byte ts[8];
  memcpy(ts + 4, bytes_.data() + 0, 4);
  memcpy(ts + 2, bytes_.data() + 4, 2);
  memcpy(ts + 0, bytes_.data() + 6, 2);
  ts[0] &= std::byte{0x0F};
  uint64_t result;
  memcpy(&result, ts, 8);
  // UUIDs are stored in network byte order.
  return detail::from_network_order(result);
}

uint16_t uuid::clock_sequence() const noexcept {
  // Read clk_seq_(hi|low) fields and strip the variant bits.
  std::byte cs[2];
  memcpy(cs, bytes_.data() + 8, 2);
  cs[0] &= std::byte{0x3F};
  uint16_t result;
  memcpy(&result, cs, 2);
  // UUIDs are stored in network byte order.
  return detail::from_network_order(result);
}

uint64_t uuid::node() const noexcept {
  std::byte n[8];
  memset(n, 0, 2);
  memcpy(n + 2, bytes_.data() + 10, 6);
  uint64_t result;
  memcpy(&result, n, 8);
  return detail::from_network_order(result);
}

size_t uuid::hash() const noexcept {
  return hash::fnv<size_t>::compute(bytes_);
}

namespace {

enum parse_result {
  valid_uuid,
  malformed_uuid,
  invalid_variant,
  invalid_version,
};

parse_result parse_impl(string_parser_state& ps, uuid::array_type& x) noexcept {
  // Create function object for writing into x.
  auto read_byte = [pos{x.data()}](auto& ps) mutable {
    uint8_t value = 0;
    auto c1 = ps.current();
    if (!isxdigit(c1) || !detail::parser::add_ascii<16>(value, c1))
      return false;
    auto c2 = ps.next();
    if (!isxdigit(c2) || !detail::parser::add_ascii<16>(value, c2))
      return false;
    ps.next();
    *pos++ = static_cast<std::byte>(value);
    return true;
  };
  // Parse the formatted string.
  ps.skip_whitespaces();
  for (int i = 0; i < 4; ++i)
    if (!read_byte(ps))
      return malformed_uuid;
  for (int block_size : {2, 2, 2, 6}) {
    if (!ps.consume_strict('-'))
      return malformed_uuid;
    for (int i = 0; i < block_size; ++i)
      if (!read_byte(ps))
        return malformed_uuid;
  }
  ps.skip_whitespaces();
  if (!ps.at_end())
    return malformed_uuid;
  // Check whether the bytes form a valid UUID.
  if (memcmp(x.data(), nil_bytes, 16) == 0)
    return valid_uuid;
  if (auto subtype = std::to_integer<long>(x[6]) >> 4;
      subtype == 0 || subtype > 5)
    return invalid_version;
  return valid_uuid;
}

} // namespace

uuid uuid::random(unsigned seed) noexcept {
  // Algorithm as defined in RFC4122:
  // - Set the two most significant bits (bits 6 and 7) of the
  //    clock_seq_hi_and_reserved to zero and one, respectively.
  // - Set the four most significant bits (bits 12 through 15) of the
  //   time_hi_and_version field to the 4-bit version number from Section 4.1.3.
  // - Set all the other bits to randomly (or pseudo-randomly) chosen values.
  // However, we first fill all bits with random data and then fix the variant
  // and version fields. It's more straightforward that way.
  std::minstd_rand engine{seed};
  std::uniform_int_distribution<> rng{0, 255};
  uuid result;
  for (size_t index = 0; index < 16; ++index)
    result.bytes_[index] = static_cast<std::byte>(rng(engine));
  result.bytes_[6] = (result.bytes_[6] & std::byte{0x0F}) | std::byte{0x50};
  result.bytes_[8] = (result.bytes_[8] & std::byte{0x3F}) | std::byte{0x80};
  return result;
}

uuid uuid::random() noexcept {
  std::random_device rd;
  return random(rd());
}

bool uuid::can_parse(std::string_view str) noexcept {
  array_type bytes;
  string_parser_state ps{str.begin(), str.end()};
  return parse_impl(ps, bytes) == valid_uuid;
}

error parse(std::string_view str, uuid& dest) {
  string_parser_state ps{str.begin(), str.end()};
  switch (parse_impl(ps, dest.bytes())) {
    case valid_uuid:
      return none;
    default:
      return make_error(ps.code);
    case invalid_version:
      return make_error(pec::invalid_argument,
                        "invalid version in variant field");
  }
}

std::string to_string(const uuid& x) {
  static constexpr auto lowercase = detail::hex_format::lowercase;
  using detail::append_hex;
  std::string result;
  append_hex<lowercase>(result, x.bytes().data() + 0, 4);
  result += '-';
  append_hex<lowercase>(result, x.bytes().data() + 4, 2);
  result += '-';
  append_hex<lowercase>(result, x.bytes().data() + 6, 2);
  result += '-';
  append_hex<lowercase>(result, x.bytes().data() + 8, 2);
  result += '-';
  append_hex<lowercase>(result, x.bytes().data() + 10, 6);
  return result;
}

expected<uuid> make_uuid(std::string_view str) {
  uuid result;
  if (auto err = parse(str, result))
    return err;
  return result;
}

} // namespace caf
