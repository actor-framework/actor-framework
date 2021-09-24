// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE binary_deserializer

#include "caf/binary_serializer.hpp"

#include "core-test.hpp"
#include "nasty.hpp"

#include <cstring>
#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/timestamp.hpp"

using namespace caf;

namespace {

byte operator"" _b(unsigned long long int x) {
  return static_cast<byte>(x);
}

byte operator"" _b(char x) {
  return static_cast<byte>(x);
}

struct arr {
  int8_t xs[3];
  int8_t operator[](size_t index) const noexcept {
    return xs[index];
  }
};

template <class Inspector>
bool inspect(Inspector& f, arr& x) {
  return f.object(x).fields(f.field("xs", x.xs));
}

struct fixture {
  template <class... Ts>
  void load(const std::vector<byte>& buf, Ts&... xs) {
    binary_deserializer source{nullptr, buf};
    if (!(source.apply(xs) && ...))
      CAF_FAIL("binary_deserializer failed to load: " << source.get_error());
  }

  template <class T>
  auto load(const std::vector<byte>& buf) {
    auto result = T{};
    load(buf, result);
    return result;
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

#define SUBTEST(msg)                                                           \
  MESSAGE(msg);                                                                \
  if (true)

#define CHECK_LOAD(type, value, ...) CHECK_EQ(load<type>({__VA_ARGS__}), value)

CAF_TEST(binary deserializer handles all primitive types) {
  SUBTEST("8-bit integers") {
    CHECK_LOAD(int8_t, 60, 0b00111100_b);
    CHECK_LOAD(int8_t, -61, 0b11000011_b);
    CHECK_LOAD(uint8_t, 60u, 0b00111100_b);
    CHECK_LOAD(uint8_t, 195u, 0b11000011_b);
  }
  SUBTEST("16-bit integers") {
    CHECK_LOAD(int16_t, 85, 0b00000000_b, 0b01010101_b);
    CHECK_LOAD(int16_t, -32683, 0b10000000_b, 0b01010101_b);
    CHECK_LOAD(uint16_t, 85u, 0b00000000_b, 0b01010101_b);
    CHECK_LOAD(uint16_t, 32853u, 0b10000000_b, 0b01010101_b);
  }
  SUBTEST("32-bit integers") {
    CHECK_LOAD(int32_t, -345, 0xFF_b, 0xFF_b, 0xFE_b, 0xA7_b);
    CHECK_LOAD(uint32_t, 4294966951u, 0xFF_b, 0xFF_b, 0xFE_b, 0xA7_b);
  }
  SUBTEST("64-bit integers") {
    CHECK_LOAD(int64_t, -1234567890123456789ll, //
               0xEE_b, 0xDD_b, 0xEF_b, 0x0B_b, 0x82_b, 0x16_b, 0x7E_b, 0xEB_b);
    CHECK_LOAD(uint64_t, 17212176183586094827llu, //
               0xEE_b, 0xDD_b, 0xEF_b, 0x0B_b, 0x82_b, 0x16_b, 0x7E_b, 0xEB_b);
  }
  SUBTEST("floating points use IEEE-754 conversion") {
    CHECK_LOAD(float, 3.45f, 0x40_b, 0x5C_b, 0xCC_b, 0xCD_b);
  }
  SUBTEST("strings use a varbyte-encoded size prefix") {
    CHECK_LOAD(std::string, "hello", 5_b, 'h'_b, 'e'_b, 'l'_b, 'l'_b, 'o'_b);
  }
  SUBTEST("enum types") {
    CHECK_LOAD(test_enum, test_enum::a, 0_b, 0_b, 0_b, 0_b);
    CHECK_LOAD(test_enum, test_enum::b, 0_b, 0_b, 0_b, 1_b);
    CHECK_LOAD(test_enum, test_enum::c, 0_b, 0_b, 0_b, 2_b);
  }
}

CAF_TEST(concatenation) {
  SUBTEST("calling f(a, b) writes a and b into the buffer in order") {
    int8_t x = 0;
    int16_t y = 0;
    load(byte_buffer({7_b, 0x80_b, 0x55_b}), x, y);
    CHECK_EQ(x, 7);
    CHECK_EQ(y, -32683);
    load(byte_buffer({0x80_b, 0x55_b, 7_b}), y, x);
    load(byte_buffer({7_b, 0x80_b, 0x55_b}), x, y);
  }
  SUBTEST("calling f(make_pair(a, b)) is equal to calling f(a, b)") {
    using i8i16_pair = std::pair<int8_t, int16_t>;
    using i16i8_pair = std::pair<int16_t, int8_t>;
    CHECK_LOAD(i8i16_pair, std::make_pair(int8_t{7}, int16_t{-32683}), //
               7_b, 0x80_b, 0x55_b);
    CHECK_LOAD(i16i8_pair, std::make_pair(int16_t{-32683}, int8_t{7}), //
               0x80_b, 0x55_b, 7_b);
  }
  SUBTEST("calling f(make_tuple(a, b)) is equivalent to f(make_pair(a, b))") {
    using i8i16_tuple = std::tuple<int8_t, int16_t>;
    using i16i8_tuple = std::tuple<int16_t, int8_t>;
    CHECK_LOAD(i8i16_tuple, std::make_tuple(int8_t{7}, int16_t{-32683}), //
               7_b, 0x80_b, 0x55_b);
    CHECK_LOAD(i16i8_tuple, std::make_tuple(int16_t{-32683}, int8_t{7}), //
               0x80_b, 0x55_b, 7_b);
  }
  SUBTEST("arrays behave like tuples") {
    arr xs{{0, 0, 0}};
    load(byte_buffer({1_b, 2_b, 3_b}), xs);
    CHECK_EQ(xs[0], 1);
    CHECK_EQ(xs[1], 2);
    CHECK_EQ(xs[2], 3);
  }
}

CAF_TEST(container types) {
  SUBTEST("STL vectors") {
    CHECK_LOAD(std::vector<int8_t>, std::vector<int8_t>({1, 2, 4, 8}), //
               4_b, 1_b, 2_b, 4_b, 8_b);
  }
  SUBTEST("STL sets") {
    CHECK_LOAD(std::set<int8_t>, std::set<int8_t>({1, 2, 4, 8}), //
               4_b, 1_b, 2_b, 4_b, 8_b);
  }
}

CAF_TEST(binary serializer picks up inspect functions) {
  SUBTEST("node ID") {
    auto nid = make_node_id(123, "000102030405060708090A0B0C0D0E0F10111213");
    CHECK_LOAD(node_id, unbox(nid),
               // content index for hashed_node_id (1)
               1_b,
               // Process ID.
               0_b, 0_b, 0_b, 123_b,
               // Host ID.
               0_b, 1_b, 2_b, 3_b, 4_b, 5_b, 6_b, 7_b, 8_b, 9_b, //
               10_b, 11_b, 12_b, 13_b, 14_b, 15_b, 16_b, 17_b, 18_b, 19_b);
  }
  SUBTEST("custom struct") {
    test_data value{-345,
                    -1234567890123456789ll,
                    3.45,
                    54.3,
                    caf::timestamp{
                      caf::timestamp::duration{1478715821 * 1000000000ll}},
                    test_enum::b,
                    "Lorem ipsum dolor sit amet."};
    CHECK_LOAD(test_data, value,
               // 32-bit i32_ member: -345
               0xFF_b, 0xFF_b, 0xFE_b, 0xA7_b,
               // 64-bit i64_ member: -1234567890123456789ll
               0xEE_b, 0xDD_b, 0xEF_b, 0x0B_b, 0x82_b, 0x16_b, 0x7E_b, 0xEB_b,
               // 32-bit f32_ member: 3.45f
               0x40_b, 0x5C_b, 0xCC_b, 0xCD_b,
               // 64-bit f64_ member: 54.3
               0x40_b, 0x4B_b, 0x26_b, 0x66_b, 0x66_b, 0x66_b, 0x66_b, 0x66_b,
               // 64-bit ts_ member.
               0x14_b, 0x85_b, 0x74_b, 0x34_b, 0x62_b, 0x74_b, 0x82_b, 0x00_b,
               // 32-bit te_ member: test_enum::b
               0x00_b, 0x00_b, 0x00_b, 0x01_b,
               // str_ member:
               0x1B_b, //
               'L'_b, 'o'_b, 'r'_b, 'e'_b, 'm'_b, ' '_b, 'i'_b, 'p'_b, 's'_b,
               'u'_b, 'm'_b, ' '_b, 'd'_b, 'o'_b, 'l'_b, 'o'_b, 'r'_b, ' '_b,
               's'_b, 'i'_b, 't'_b, ' '_b, 'a'_b, 'm'_b, 'e'_b, 't'_b, '.'_b);
  }
  SUBTEST("enum class with non-default overload") {
    auto day = weekday::friday;
    CHECK_LOAD(weekday, day, 0x04_b);
  }
}

END_FIXTURE_SCOPE()
