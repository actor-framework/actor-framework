// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE byte

#include "caf/byte.hpp"

#include "core-test.hpp"

#include <cstdint>

#include "caf/detail/parser/add_ascii.hpp"
#include "caf/raise_error.hpp"

using namespace caf;

namespace {

byte operator"" _b(const char* str, size_t n) {
  size_t consumed = 0;
  uint8_t result = 0;
  for (size_t i = 0; i < n; ++i) {
    if (str[i] != '\'') {
      if (!detail::parser::add_ascii<2>(result, str[i]))
        CAF_RAISE_ERROR(std::logic_error,
                        "invalid character or over-/underflow");
      else
        ++consumed;
    }
  }
  if (consumed != 8)
    CAF_RAISE_ERROR(std::logic_error, "too few digits, expected exactly 8");
  return static_cast<byte>(result);
}

struct fixture {
  fixture() {
    // Sanity checks.
    if ("0001'1100"_b != static_cast<byte>(0x1C))
      CAF_FAIL("operator \"\"_b broken");
    if ("1000'0001"_b != static_cast<byte>(0x81))
      CAF_FAIL("operator \"\"_b broken");
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(byte_tests, fixture)

CAF_TEST(to integer) {
  CAF_CHECK_EQUAL(to_integer<int>("0110'1001"_b), 0x69);
}

CAF_TEST(left shift) {
  auto x = "0000'0001"_b;
  x <<= 1;
  CAF_CHECK_EQUAL(x, "0000'0010"_b);
  CAF_CHECK_EQUAL("0000'0010"_b << 1, "0000'0100"_b);
  CAF_CHECK_EQUAL("0000'0010"_b << 2, "0000'1000"_b);
  CAF_CHECK_EQUAL("0000'0010"_b << 3, "0001'0000"_b);
  CAF_CHECK_EQUAL("0000'0010"_b << 4, "0010'0000"_b);
  CAF_CHECK_EQUAL("0000'0010"_b << 5, "0100'0000"_b);
  CAF_CHECK_EQUAL("0000'0010"_b << 6, "1000'0000"_b);
  CAF_CHECK_EQUAL("0000'0010"_b << 7, "0000'0000"_b);
}

CAF_TEST(right shift) {
  auto x = "0100'0000"_b;
  x >>= 1;
  CAF_CHECK_EQUAL(x, "0010'0000"_b);
  CAF_CHECK_EQUAL("0100'0000"_b >> 1, "0010'0000"_b);
  CAF_CHECK_EQUAL("0100'0000"_b >> 2, "0001'0000"_b);
  CAF_CHECK_EQUAL("0100'0000"_b >> 3, "0000'1000"_b);
  CAF_CHECK_EQUAL("0100'0000"_b >> 4, "0000'0100"_b);
  CAF_CHECK_EQUAL("0100'0000"_b >> 5, "0000'0010"_b);
  CAF_CHECK_EQUAL("0100'0000"_b >> 6, "0000'0001"_b);
  CAF_CHECK_EQUAL("0100'0000"_b >> 7, "0000'0000"_b);
}

CAF_TEST(bitwise or) {
  auto x = "0001'1110"_b;
  x |= "0111'1000"_b;
  CAF_CHECK_EQUAL(x, "0111'1110"_b);
  CAF_CHECK_EQUAL("0001'1110"_b | "0111'1000"_b, "0111'1110"_b);
}

CAF_TEST(bitwise and) {
  auto x = "0001'1110"_b;
  x &= "0111'1000"_b;
  CAF_CHECK_EQUAL(x, "0001'1000"_b);
  CAF_CHECK_EQUAL("0001'1110"_b & "0111'1000"_b, "0001'1000"_b);
}

CAF_TEST(bitwise xor) {
  auto x = "0001'1110"_b;
  x ^= "0111'1000"_b;
  CAF_CHECK_EQUAL(x, "0110'0110"_b);
  CAF_CHECK_EQUAL("0001'1110"_b ^ "0111'1000"_b, "0110'0110"_b);
}

CAF_TEST(bitwise not) {
  CAF_CHECK_EQUAL(~"0111'1110"_b, "1000'0001"_b);
}

CAF_TEST_FIXTURE_SCOPE_END()
