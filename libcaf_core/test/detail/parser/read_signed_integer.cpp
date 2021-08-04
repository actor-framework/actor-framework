// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.parser.read_signed_integer

#include "caf/detail/parser/read_signed_integer.hpp"

#include "caf/test/dsl.hpp"

#include "caf/parser_state.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

using namespace caf;

namespace {

template <class T>
struct signed_integer_consumer {
  using value_type = T;

  void value(T y) {
    x = y;
  }

  T x;
};

template <class T>
std::optional<T> read(string_view str) {
  signed_integer_consumer<T> consumer;
  string_parser_state ps{str.begin(), str.end()};
  detail::parser::read_signed_integer(ps, consumer);
  if (ps.code != pec::success)
    return std::nullopt;
  return consumer.x;
}

template <class T>
bool underflow(string_view str) {
  signed_integer_consumer<T> consumer;
  string_parser_state ps{str.begin(), str.end()};
  detail::parser::read_signed_integer(ps, consumer);
  return ps.code == pec::integer_underflow;
}

template <class T>
bool overflow(string_view str) {
  signed_integer_consumer<T> consumer;
  string_parser_state ps{str.begin(), str.end()};
  detail::parser::read_signed_integer(ps, consumer);
  return ps.code == pec::integer_overflow;
}

template <class T>
T min_val() {
  return std::numeric_limits<T>::min();
}

template <class T>
T max_val() {
  return std::numeric_limits<T>::max();
}

} // namespace

#define ZERO_VALUE(type, literal)                                              \
  CAF_CHECK_EQUAL(read<type>(#literal), type(0));

#define MAX_VALUE(type, literal)                                               \
  CAF_CHECK_EQUAL(read<type>(#literal), max_val<type>());

#define MIN_VALUE(type, literal)                                               \
  CAF_CHECK_EQUAL(read<type>(#literal), min_val<type>());

#ifdef OVERFLOW
#  undef OVERFLOW
#endif // OVERFLOW

#define OVERFLOW(type, literal) CAF_CHECK(overflow<type>(#literal));

#ifdef UNDERFLOW
#  undef UNDERFLOW
#endif // UNDERFLOW

#define UNDERFLOW(type, literal) CAF_CHECK(underflow<type>(#literal));

CAF_TEST(read zeros) {
  ZERO_VALUE(int8_t, 0);
  ZERO_VALUE(int8_t, 00);
  ZERO_VALUE(int8_t, 0x0);
  ZERO_VALUE(int8_t, 0X00);
  ZERO_VALUE(int8_t, 0b0);
  ZERO_VALUE(int8_t, 0B00);
  ZERO_VALUE(int8_t, +0);
  ZERO_VALUE(int8_t, +00);
  ZERO_VALUE(int8_t, +0x0);
  ZERO_VALUE(int8_t, +0X00);
  ZERO_VALUE(int8_t, +0b0);
  ZERO_VALUE(int8_t, +0B00);
  ZERO_VALUE(int8_t, -0);
  ZERO_VALUE(int8_t, -00);
  ZERO_VALUE(int8_t, -0x0);
  ZERO_VALUE(int8_t, -0X00);
  ZERO_VALUE(int8_t, -0b0);
  ZERO_VALUE(int8_t, -0B00);
}

CAF_TEST(minimal value) {
  MIN_VALUE(int8_t, -0b10000000);
  MIN_VALUE(int8_t, -0200);
  MIN_VALUE(int8_t, -128);
  MIN_VALUE(int8_t, -0x80);
  UNDERFLOW(int8_t, -0b10000001);
  UNDERFLOW(int8_t, -0201);
  UNDERFLOW(int8_t, -129);
  UNDERFLOW(int8_t, -0x81);
  MIN_VALUE(int16_t, -0b1000000000000000);
  MIN_VALUE(int16_t, -0100000);
  MIN_VALUE(int16_t, -32768);
  MIN_VALUE(int16_t, -0x8000);
  UNDERFLOW(int16_t, -0b1000000000000001);
  UNDERFLOW(int16_t, -0100001);
  UNDERFLOW(int16_t, -32769);
  UNDERFLOW(int16_t, -0x8001);
  MIN_VALUE(int32_t, -0b10000000000000000000000000000000);
  MIN_VALUE(int32_t, -020000000000);
  MIN_VALUE(int32_t, -2147483648);
  MIN_VALUE(int32_t, -0x80000000);
  UNDERFLOW(int32_t, -0b10000000000000000000000000000001);
  UNDERFLOW(int32_t, -020000000001);
  UNDERFLOW(int32_t, -2147483649);
  UNDERFLOW(int32_t, -0x80000001);
  MIN_VALUE(int64_t, -01000000000000000000000);
  MIN_VALUE(int64_t, -9223372036854775808);
  MIN_VALUE(int64_t, -0x8000000000000000);
  UNDERFLOW(int64_t, -01000000000000000000001);
  UNDERFLOW(int64_t, -9223372036854775809);
  UNDERFLOW(int64_t, -0x8000000000000001);
}

CAF_TEST(maximal value) {
  MAX_VALUE(int8_t, 0b1111111);
  MAX_VALUE(int8_t, 0177);
  MAX_VALUE(int8_t, 127);
  MAX_VALUE(int8_t, 0x7F);
  OVERFLOW(int8_t, 0b10000000);
  OVERFLOW(int8_t, 0200);
  OVERFLOW(int8_t, 128);
  OVERFLOW(int8_t, 0x80);
  MAX_VALUE(int16_t, 0b111111111111111);
  MAX_VALUE(int16_t, 077777);
  MAX_VALUE(int16_t, 32767);
  MAX_VALUE(int16_t, 0x7FFF);
  OVERFLOW(int16_t, 0b1000000000000000);
  OVERFLOW(int16_t, 0100000);
  OVERFLOW(int16_t, 32768);
  OVERFLOW(int16_t, 0x8000);
  MAX_VALUE(int32_t, 0b1111111111111111111111111111111);
  MAX_VALUE(int32_t, 017777777777);
  MAX_VALUE(int32_t, 2147483647);
  MAX_VALUE(int32_t, 0x7FFFFFFF);
  OVERFLOW(int32_t, 0b10000000000000000000000000000000);
  OVERFLOW(int32_t, 020000000000);
  OVERFLOW(int32_t, 2147483648);
  OVERFLOW(int32_t, 0x80000000);
  MAX_VALUE(int64_t,
            0b111111111111111111111111111111111111111111111111111111111111111);
  MAX_VALUE(int64_t, 0777777777777777777777);
  MAX_VALUE(int64_t, 9223372036854775807);
  MAX_VALUE(int64_t, 0x7FFFFFFFFFFFFFFF);
  OVERFLOW(int64_t,
           0b1000000000000000000000000000000000000000000000000000000000000000);
  OVERFLOW(int64_t, 01000000000000000000000);
  OVERFLOW(int64_t, 9223372036854775808);
  OVERFLOW(int64_t, 0x8000000000000000);
}
