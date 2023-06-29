// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.rfc3629

#include "caf/detail/rfc3629.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

bool valid_utf8(const_byte_span bytes) noexcept {
  return detail::rfc3629::valid(bytes);
}

bool valid_utf8(std::string_view bytes) noexcept {
  return detail::rfc3629::valid(bytes);
}

constexpr std::byte operator""_b(unsigned long long x) {
  return static_cast<std::byte>(x);
}

// Missing continuation byte.
constexpr std::byte invalid_two_byte_1[] = {0xc8_b};

// Illegal non-shortest form (1).
constexpr std::byte invalid_two_byte_2[] = {0xc0_b, 0x80_b};

// Illegal non-shortest form (2).
constexpr std::byte invalid_two_byte_3[] = {0xc1_b, 0x80_b};

// Invalid continuation byte.
constexpr std::byte invalid_two_byte_4[] = {0xc8_b, 0x0f_b};

// Missing continuation bytes.
constexpr std::byte invalid_three_byte_1[] = {0xe8_b};

// Missing continuation byte.
constexpr std::byte invalid_three_byte_2[] = {0xe8_b, 0x80_b};

// Invalid continuation byte (1).
constexpr std::byte invalid_three_byte_3[] = {0xe8_b, 0x0f_b, 0x80_b};

// Invalid continuation byte (2).
constexpr std::byte invalid_three_byte_4[] = {0xe8_b, 0x80_b, 0x0f_b};

// Illegal non-shortest form (1).
constexpr std::byte invalid_three_byte_5[] = {0xe0_b, 0x80_b, 0x80_b};

// Illegal non-shortest form (2).
constexpr std::byte invalid_three_byte_6[] = {0xe0_b, 0x9f_b, 0x8f_b};

// Illegal surrogate (smallest).
constexpr std::byte invalid_three_byte_7[] = {0xed_b, 0xa0_b, 0x80_b};

// Illegal surrogate (largest).
constexpr std::byte invalid_three_byte_8[] = {0xed_b, 0xbf_b, 0xbf_b};

// Missing continuation bytes.
constexpr std::byte invalid_four_byte_1[] = {0xf1_b};

// Missing continuation bytes 3 and 4.
constexpr std::byte invalid_four_byte_2[] = {0xf1_b, 0xbc_b};

// Missing continuation byte 4.
constexpr std::byte invalid_four_byte_3[] = {0xf1_b, 0xbc_b, 0xbc_b};

// Invalid continuation byte (1).
constexpr std::byte invalid_four_byte_4[] = {0xf1_b, 0x08_b, 0x80_b, 0x80_b};

// Invalid continuation byte (2).
constexpr std::byte invalid_four_byte_5[] = {0xf1_b, 0x80_b, 0x08_b, 0x80_b};

// Invalid continuation byte (3).
constexpr std::byte invalid_four_byte_6[] = {0xf1_b, 0x80_b, 0x80_b, 0x08_b};

// Illegal non-shortest form.
constexpr std::byte invalid_four_byte_7[] = {0xf0_b, 0x8f_b, 0x8f_b, 0x8f_b};

// Illegal start of a sequence.
constexpr std::byte invalid_four_byte_8[] = {0xf8_b, 0x80_b, 0x80_b, 0x80_b};

// Smallest valid 2-byte sequence.
constexpr std::byte valid_two_byte_1[] = {0xc2_b, 0x80_b};

// Largest valid 2-byte sequence.
constexpr std::byte valid_two_byte_2[] = {0xdf_b, 0xbf_b};

// Smallest valid 3-byte sequence.
constexpr std::byte valid_three_byte_1[] = {0xe0_b, 0xa0_b, 0x80_b};

// Largest valid 3-byte sequence.
constexpr std::byte valid_three_byte_2[] = {0xef_b, 0xbf_b, 0xbf_b};

// Smallest valid 4-byte sequence.
constexpr std::byte valid_four_byte_1[] = {0xf0_b, 0x90_b, 0x80_b, 0x80_b};

// Largest valid 4-byte sequence.
constexpr std::byte valid_four_byte_2[] = {0xf7_b, 0xbf_b, 0xbf_b, 0xbf_b};

// Single line ASCII text.
constexpr std::string_view ascii_1 = "Hello World!";

// Multi-line ASCII text.
constexpr std::string_view ascii_2 = R"__(
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    CAF                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
)__";

} // namespace

TEST_CASE("ASCII input") {
  CHECK(valid_utf8(ascii_1));
  CHECK(valid_utf8(ascii_2));
}

TEST_CASE("valid UTF-8 input") {
  CHECK(valid_utf8(valid_two_byte_1));
  CHECK(valid_utf8(valid_two_byte_2));
  CHECK(valid_utf8(valid_three_byte_1));
  CHECK(valid_utf8(valid_three_byte_2));
  CHECK(valid_utf8(valid_four_byte_1));
  CHECK(valid_utf8(valid_four_byte_2));
}

TEST_CASE("invalid UTF-8 input") {
  CHECK(!valid_utf8(invalid_two_byte_1));
  CHECK(!valid_utf8(invalid_two_byte_2));
  CHECK(!valid_utf8(invalid_two_byte_3));
  CHECK(!valid_utf8(invalid_two_byte_4));
  CHECK(!valid_utf8(invalid_three_byte_1));
  CHECK(!valid_utf8(invalid_three_byte_2));
  CHECK(!valid_utf8(invalid_three_byte_3));
  CHECK(!valid_utf8(invalid_three_byte_4));
  CHECK(!valid_utf8(invalid_three_byte_5));
  CHECK(!valid_utf8(invalid_three_byte_6));
  CHECK(!valid_utf8(invalid_three_byte_7));
  CHECK(!valid_utf8(invalid_three_byte_8));
  CHECK(!valid_utf8(invalid_four_byte_1));
  CHECK(!valid_utf8(invalid_four_byte_2));
  CHECK(!valid_utf8(invalid_four_byte_3));
  CHECK(!valid_utf8(invalid_four_byte_4));
  CHECK(!valid_utf8(invalid_four_byte_5));
  CHECK(!valid_utf8(invalid_four_byte_6));
  CHECK(!valid_utf8(invalid_four_byte_7));
  CHECK(!valid_utf8(invalid_four_byte_8));
}
