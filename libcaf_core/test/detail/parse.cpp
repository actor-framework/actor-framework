// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parse

#include "caf/detail/parse.hpp"

#include "caf/test/dsl.hpp"

#include "caf/expected.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv4_subnet.hpp"
#include "caf/ipv6_address.hpp"
#include "caf/ipv6_endpoint.hpp"
#include "caf/ipv6_subnet.hpp"
#include "caf/string_view.hpp"
#include "caf/uri.hpp"

using namespace caf;

namespace {

using std::chrono::duration_cast;

timespan operator"" _ns(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::nanoseconds(x));
}

timespan operator"" _us(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::microseconds(x));
}

timespan operator"" _ms(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::milliseconds(x));
}

timespan operator"" _s(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::seconds(x));
}

timespan operator"" _h(unsigned long long x) {
  return duration_cast<timespan>(std::chrono::hours(x));
}

template <class T>
expected<T> read(string_view str) {
  T result;
  string_parser_state ps{str.begin(), str.end()};
  detail::parse(ps, result);
  if (ps.code == pec::success)
    return result;
  return make_error(ps);
}

} // namespace

#define CHECK_NUMBER(type, value)                                              \
  CAF_CHECK_EQUAL(read<type>(#value), type(value))

#define CHECK_NUMBER_3(type, value, cpp_value)                                 \
  CAF_CHECK_EQUAL(read<type>(#value), type(cpp_value))

#define CHECK_INVALID(type, str, code) CAF_CHECK_EQUAL(read<type>(str), code)

CAF_TEST(valid signed integers) {
  CHECK_NUMBER(int8_t, -128);
  CHECK_NUMBER(int8_t, 127);
  CHECK_NUMBER(int8_t, +127);
  CHECK_NUMBER(int16_t, -32768);
  CHECK_NUMBER(int16_t, 32767);
  CHECK_NUMBER(int16_t, +32767);
  CHECK_NUMBER(int32_t, -2147483648);
  CHECK_NUMBER(int32_t, 2147483647);
  CHECK_NUMBER(int32_t, +2147483647);
  CHECK_NUMBER(int64_t, -9223372036854775807);
  CHECK_NUMBER(int64_t, 9223372036854775807);
  CHECK_NUMBER(int64_t, +9223372036854775807);
}

CAF_TEST(invalid signed integers) {
  CHECK_INVALID(int8_t, "--1", pec::unexpected_character);
  CHECK_INVALID(int8_t, "++1", pec::unexpected_character);
  CHECK_INVALID(int8_t, "-129", pec::integer_underflow);
  CHECK_INVALID(int8_t, "128", pec::integer_overflow);
  CHECK_INVALID(int8_t, "~1", pec::unexpected_character);
  CHECK_INVALID(int8_t, "1!", pec::trailing_character);
  CHECK_INVALID(int8_t, "+", pec::unexpected_eof);
  CHECK_INVALID(int8_t, "-", pec::unexpected_eof);
}

CAF_TEST(valid unsigned integers) {
  CHECK_NUMBER(uint8_t, 0);
  CHECK_NUMBER(uint8_t, +0);
  CHECK_NUMBER(uint8_t, 255);
  CHECK_NUMBER(uint8_t, +255);
  CHECK_NUMBER(uint16_t, 0);
  CHECK_NUMBER(uint16_t, +0);
  CHECK_NUMBER(uint16_t, 65535);
  CHECK_NUMBER(uint16_t, +65535);
  CHECK_NUMBER(uint32_t, 0);
  CHECK_NUMBER(uint32_t, +0);
  CHECK_NUMBER(uint32_t, 4294967295);
  CHECK_NUMBER(uint32_t, +4294967295);
  CHECK_NUMBER(uint64_t, 0);
  CHECK_NUMBER(uint64_t, +0);
  CHECK_NUMBER_3(uint64_t, 18446744073709551615, 18446744073709551615ULL);
  CHECK_NUMBER_3(uint64_t, +18446744073709551615, 18446744073709551615ULL);
}

CAF_TEST(invalid unsigned integers) {
  CHECK_INVALID(uint8_t, "-1", pec::unexpected_character);
  CHECK_INVALID(uint8_t, "++1", pec::unexpected_character);
  CHECK_INVALID(uint8_t, "256", pec::integer_overflow);
  CHECK_INVALID(uint8_t, "~1", pec::unexpected_character);
  CHECK_INVALID(uint8_t, "1!", pec::trailing_character);
  CHECK_INVALID(uint8_t, "+", pec::unexpected_eof);
}

CAF_TEST(valid floating point numbers) {
  CHECK_NUMBER(float, 1);
  CHECK_NUMBER(double, 1);
  CHECK_NUMBER(double, 0.01e10);
  CHECK_NUMBER(double, 10e-10);
  CHECK_NUMBER(double, -10e-10);
}

CAF_TEST(invalid floating point numbers) {
  CHECK_INVALID(float, "1..", pec::trailing_character);
  CHECK_INVALID(double, "..1", pec::unexpected_character);
  CHECK_INVALID(double, "+", pec::unexpected_eof);
  CHECK_INVALID(double, "-", pec::unexpected_eof);
  CHECK_INVALID(double, "1e", pec::unexpected_eof);
  CHECK_INVALID(double, "--0.01e10", pec::unexpected_character);
  CHECK_INVALID(double, "++10e-10", pec::unexpected_character);
}

CAF_TEST(valid timespans) {
  CAF_CHECK_EQUAL(read<timespan>("12ns"), 12_ns);
  CAF_CHECK_EQUAL(read<timespan>("34us"), 34_us);
  CAF_CHECK_EQUAL(read<timespan>("56ms"), 56_ms);
  CAF_CHECK_EQUAL(read<timespan>("78s"), 78_s);
  CAF_CHECK_EQUAL(read<timespan>("60min"), 1_h);
  CAF_CHECK_EQUAL(read<timespan>("90h"), 90_h);
}

CAF_TEST(invalid timespans) {
  CAF_CHECK_EQUAL(read<timespan>("12"), pec::unexpected_eof);
  CAF_CHECK_EQUAL(read<timespan>("12nas"), pec::unexpected_character);
  CAF_CHECK_EQUAL(read<timespan>("34usec"), pec::trailing_character);
  CAF_CHECK_EQUAL(read<timespan>("56m"), pec::unexpected_eof);
}

CAF_TEST(strings) {
  CAF_CHECK_EQUAL(read<std::string>("    foo\t  "), "foo");
  CAF_CHECK_EQUAL(read<std::string>("  \"  foo\t\"  "), "  foo\t");
}

CAF_TEST(uris) {
  if (auto x_res = read<uri>("foo:bar")) {
    auto x = *x_res;
    CAF_CHECK_EQUAL(x.scheme(), "foo");
    CAF_CHECK_EQUAL(x.path(), "bar");
  } else {
    CAF_ERROR("my:path not recognized as URI");
  }
}

CAF_TEST(IPv4 address) {
  CAF_CHECK_EQUAL(read<ipv4_address>("1.2.3.4"), ipv4_address({1, 2, 3, 4}));
  CAF_CHECK_EQUAL(read<ipv4_address>("127.0.0.1"),
                  ipv4_address({127, 0, 0, 1}));
  CAF_CHECK_EQUAL(read<ipv4_address>("256.0.0.1"), pec::integer_overflow);
}

CAF_TEST(IPv4 subnet) {
  CAF_CHECK_EQUAL(read<ipv4_subnet>("1.2.3.0/24"),
                  ipv4_subnet(ipv4_address({1, 2, 3, 0}), 24));
  CAF_CHECK_EQUAL(read<ipv4_subnet>("1.2.3.0/33"), pec::integer_overflow);
}

CAF_TEST(IPv4 endpoint) {
  CAF_CHECK_EQUAL(read<ipv4_endpoint>("127.0.0.1:0"),
                  ipv4_endpoint(ipv4_address({127, 0, 0, 1}), 0));
  CAF_CHECK_EQUAL(read<ipv4_endpoint>("127.0.0.1:65535"),
                  ipv4_endpoint(ipv4_address({127, 0, 0, 1}), 65535));
  CAF_CHECK_EQUAL(read<ipv4_endpoint>("127.0.0.1:65536"),
                  pec::integer_overflow);
}

CAF_TEST(IPv6 address) {
  CAF_CHECK_EQUAL(read<ipv6_address>("1.2.3.4"), ipv4_address({1, 2, 3, 4}));
  CAF_CHECK_EQUAL(read<ipv6_address>("1::"), ipv6_address({{1}, {}}));
  CAF_CHECK_EQUAL(read<ipv6_address>("::2"), ipv6_address({{}, {2}}));
  CAF_CHECK_EQUAL(read<ipv6_address>("1::2"), ipv6_address({{1}, {2}}));
}

CAF_TEST(IPv6 subnet) {
  CAF_CHECK_EQUAL(read<ipv6_subnet>("1.2.3.0/24"),
                  ipv6_subnet(ipv4_address({1, 2, 3, 0}), 24));
  CAF_CHECK_EQUAL(read<ipv6_subnet>("1::/128"),
                  ipv6_subnet(ipv6_address({1}, {}), 128));
  CAF_CHECK_EQUAL(read<ipv6_subnet>("1::/129"), pec::integer_overflow);
}

CAF_TEST(IPv6 endpoint) {
  CAF_CHECK_EQUAL(read<ipv6_endpoint>("127.0.0.1:0"),
                  ipv6_endpoint(ipv4_address({127, 0, 0, 1}), 0));
  CAF_CHECK_EQUAL(read<ipv6_endpoint>("127.0.0.1:65535"),
                  ipv6_endpoint(ipv4_address({127, 0, 0, 1}), 65535));
  CAF_CHECK_EQUAL(read<ipv6_endpoint>("127.0.0.1:65536"),
                  pec::integer_overflow);
  CAF_CHECK_EQUAL(read<ipv6_endpoint>("[1::2]:8080"),
                  ipv6_endpoint({{1}, {2}}, 8080));
}
