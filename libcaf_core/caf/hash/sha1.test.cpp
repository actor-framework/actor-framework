// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/hash/sha1.hpp"

#include "caf/test/test.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>

using namespace std::literals;

using caf::hash::sha1;

namespace {

template <class... Ts>
auto make_bytes(Ts... xs) {
  static_assert(sizeof...(Ts) == 20);
  return sha1::result_type{{static_cast<std::byte>(xs)...}};
}

} // namespace

TEST("strings are hashed by their content only") {
  auto str = "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11"sv;
  check_eq(sha1::compute(str),
           make_bytes(0xb3, 0x7a, 0x4f, 0x2c,   // Bytes  1 -  4
                      0xc0, 0x62, 0x4f, 0x16,   //        5 -  8
                      0x90, 0xf6, 0x46, 0x06,   //        9 - 12
                      0xcf, 0x38, 0x59, 0x45,   //       13 - 16
                      0xb2, 0xbe, 0xc4, 0xea)); //       17 - 20
}

TEST("sha1::result returns the same digest when called multiple times") {
  sha1 f;
  f.value(42);
  auto r1 = f.result();
  auto r2 = f.result();
  check_eq(r1, r2);
}

TEST("sha1 rejects appending after result has been called") {
  sha1 f;
  f.value(1);
  static_cast<void>(f.result());
  f.value(2);
  check(!f.get_error().empty());
}

TEST("sha1 hashes messages that require an extra padding block") {
  std::string msg(56, 'x');
  auto digest = sha1::compute(msg);
  sha1 f;
  for (char c : msg)
    f.value(static_cast<uint8_t>(c));
  check_eq(f.result(), digest);
}

TEST("sha1 hashes messages that fill a full block") {
  std::string msg(64, 'a');
  auto digest = sha1::compute(msg);
  sha1 f;
  for (char c : msg)
    f.value(static_cast<uint8_t>(c));
  check_eq(f.result(), digest);
}

TEST("sha1 hashes bool float and double deterministically") {
  auto h_bool_true = sha1::compute(true);
  auto h_bool_false = sha1::compute(false);
  check_ne(h_bool_true, h_bool_false);
  auto h_float = sha1::compute(1.0f);
  auto h_float2 = sha1::compute(1.0f);
  check_eq(h_float, h_float2);
  auto h_double = sha1::compute(1.0);
  auto h_double2 = sha1::compute(1.0);
  check_eq(h_double, h_double2);
}

TEST("sha1 hashes byte spans") {
  auto bytes = std::array<std::byte, 4>{
    {std::byte{'a'}, std::byte{'b'}, std::byte{'c'}, std::byte{'d'}}};
  auto from_span = sha1::compute(caf::const_byte_span(bytes));
  auto from_string = sha1::compute("abcd"sv);
  check_eq(from_span, from_string);
}

TEST("sha1 error state can be set and read") {
  sha1 f;
  check(f.get_error().empty());
  f.set_error(caf::make_error(caf::sec::runtime_error, "test error"));
  check(!f.get_error().empty());
}

TEST("sha1 hashes optional values") {
  auto with_value = sha1::compute(std::optional<int>{42});
  auto empty = sha1::compute(std::optional<int>{});
  check_ne(with_value, empty);
}

TEST("sha1 hashes variant values") {
  auto as_int = sha1::compute(std::variant<int, std::string>{42});
  auto as_string
    = sha1::compute(std::variant<int, std::string>{std::string{"42"}});
  check_ne(as_int, as_string);
}
