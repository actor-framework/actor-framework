// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/rfc3629.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using detail::rfc3629;

using res_t = std::pair<size_t, bool>;

SUITE("detail.rfc3629") {

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

// UTF-8 standard covers code points in the sequences [0x0 - 0x110000)
// Theoretically, a larger value can be encoded in the 4-byte sequence.
// Smallest valid 4-byte sequence.
constexpr std::byte valid_four_byte_1[] = {0xf0_b, 0x90_b, 0x80_b, 0x80_b};

// Largest valid 4-byte sequence - code point 0x10FFFF.
constexpr std::byte valid_four_byte_2[] = {0xf4_b, 0x8f_b, 0xbf_b, 0xbf_b};

// Smallest invalid 4-byte sequence - code point 0x110000.
constexpr std::byte invalid_four_byte_9[] = {0xf4_b, 0x90_b, 0x80_b, 0x80_b};

// Largest invalid 4-byte sequence - invalid code point.
constexpr std::byte invalid_four_byte_10[] = {0xf7_b, 0xbf_b, 0xbf_b, 0xbf_b};

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

TEST("rfc3629::valid checks whether an input is valid UTF-8") {
  SECTION("valid ASCII input") {
    check(valid_utf8(ascii_1));
    check(valid_utf8(ascii_2));
  }
  SECTION("valid UTF-8 input") {
    check(valid_utf8(valid_two_byte_1));
    check(valid_utf8(valid_two_byte_2));
    check(valid_utf8(valid_three_byte_1));
    check(valid_utf8(valid_three_byte_2));
    check(valid_utf8(valid_four_byte_1));
    check(valid_utf8(valid_four_byte_2));
  }
  SECTION("invalid UTF-8 input") {
    check(!valid_utf8(invalid_two_byte_1));
    check(!valid_utf8(invalid_two_byte_2));
    check(!valid_utf8(invalid_two_byte_3));
    check(!valid_utf8(invalid_two_byte_4));
    check(!valid_utf8(invalid_three_byte_1));
    check(!valid_utf8(invalid_three_byte_2));
    check(!valid_utf8(invalid_three_byte_3));
    check(!valid_utf8(invalid_three_byte_4));
    check(!valid_utf8(invalid_three_byte_5));
    check(!valid_utf8(invalid_three_byte_6));
    check(!valid_utf8(invalid_three_byte_7));
    check(!valid_utf8(invalid_three_byte_8));
    check(!valid_utf8(invalid_four_byte_1));
    check(!valid_utf8(invalid_four_byte_2));
    check(!valid_utf8(invalid_four_byte_3));
    check(!valid_utf8(invalid_four_byte_4));
    check(!valid_utf8(invalid_four_byte_5));
    check(!valid_utf8(invalid_four_byte_6));
    check(!valid_utf8(invalid_four_byte_7));
    check(!valid_utf8(invalid_four_byte_8));
    check(!valid_utf8(invalid_four_byte_9));
    check(!valid_utf8(invalid_four_byte_10));
  }
}

TEST("rfc3629::validate returns the end index if the range is valid") {
  SECTION("valid ASCII input") {
    check_eq(rfc3629::validate(ascii_1), res_t{ascii_1.size(), false});
    check_eq(rfc3629::validate(ascii_2), res_t{ascii_2.size(), false});
  }
  SECTION("valid UTF-8 input") {
    check_eq(rfc3629::validate(valid_two_byte_1), res_t{2, false});
    check_eq(rfc3629::validate(valid_two_byte_2), res_t{2, false});
    check_eq(rfc3629::validate(valid_three_byte_1), res_t{3, false});
    check_eq(rfc3629::validate(valid_three_byte_2), res_t{3, false});
    check_eq(rfc3629::validate(valid_four_byte_1), res_t{4, false});
    check_eq(rfc3629::validate(valid_four_byte_2), res_t{4, false});
  }
}

TEST("rfc3629::validate stops at the first invalid byte") {
  SECTION("UTF-8 input missing continuation bytes") {
    check_eq(rfc3629::validate(invalid_two_byte_1), res_t{0, true});
    check_eq(rfc3629::validate(invalid_three_byte_1), res_t{0, true});
    check_eq(rfc3629::validate(invalid_three_byte_2), res_t{0, true});
    check_eq(rfc3629::validate(invalid_four_byte_1), res_t{0, true});
    check_eq(rfc3629::validate(invalid_four_byte_2), res_t{0, true});
    check_eq(rfc3629::validate(invalid_four_byte_3), res_t{0, true});
  }
  SECTION("UTF-8 input with malformed data") {
    check_eq(rfc3629::validate(invalid_two_byte_2), res_t{0, false});
    check_eq(rfc3629::validate(invalid_two_byte_3), res_t{0, false});
    check_eq(rfc3629::validate(invalid_two_byte_4), res_t{0, false});
    check_eq(rfc3629::validate(invalid_three_byte_3), res_t{0, false});
    check_eq(rfc3629::validate(invalid_three_byte_4), res_t{0, false});
    check_eq(rfc3629::validate(invalid_three_byte_5), res_t{0, false});
    check_eq(rfc3629::validate(invalid_three_byte_6), res_t{0, false});
    check_eq(rfc3629::validate(invalid_three_byte_7), res_t{0, false});
    check_eq(rfc3629::validate(invalid_three_byte_8), res_t{0, false});
    check_eq(rfc3629::validate(invalid_four_byte_4), res_t{0, false});
    check_eq(rfc3629::validate(invalid_four_byte_5), res_t{0, false});
    check_eq(rfc3629::validate(invalid_four_byte_6), res_t{0, false});
    check_eq(rfc3629::validate(invalid_four_byte_7), res_t{0, false});
    check_eq(rfc3629::validate(invalid_four_byte_8), res_t{0, false});
    check_eq(rfc3629::validate(invalid_four_byte_9), res_t{0, false});
    check_eq(rfc3629::validate(invalid_four_byte_10), res_t{0, false});
  }
  SECTION("invalid UTF-8 input fails on the invalid byte") {
    check_eq(rfc3629::validate(make_span(invalid_four_byte_9, 2)),
             res_t{0, false});
    check_eq(rfc3629::validate(make_span(invalid_four_byte_10, 1)),
             res_t{0, false});
  }
  SECTION("invalid UTF-8 input with valid prefix") {
    byte_buffer data;
    data.insert(data.end(), begin(valid_four_byte_1), end(valid_four_byte_1));
    data.insert(data.end(), begin(valid_four_byte_2), end(valid_four_byte_2));
    data.insert(data.end(), begin(valid_two_byte_1), end(valid_two_byte_1));
    data.insert(data.end(), begin(invalid_four_byte_4),
                end(invalid_four_byte_4));
    check_eq(rfc3629::validate(data), res_t{10, false});
  }
}

} // SUITE("detail.rfc3629")
