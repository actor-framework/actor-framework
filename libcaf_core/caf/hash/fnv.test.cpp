// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/hash/fnv.hpp"

#include "caf/test/test.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>

using namespace std::literals;

TEST("hash::fnv can build hash values incrementally") {
  caf::hash::fnv<uint32_t> f;
  check_eq(f.result, 0x811C9DC5u);
  f.value('a');
  check_eq(f.result, 0xE40C292Cu);
  f.value('b');
  check_eq(f.result, 0x4D2505CAu);
  f.value('c');
  check_eq(f.result, 0x1A47E90Bu);
  f.value('d');
  check_eq(f.result, 0xCE3479BDu);
}

TEST("hash::fnv hashes std::byte like the corresponding unsigned integer") {
  caf::hash::fnv<uint32_t> f_byte;
  f_byte.value(std::byte{'a'});
  caf::hash::fnv<uint32_t> f_uint8;
  f_uint8.value(static_cast<uint8_t>('a'));
  check_eq(f_byte.result, f_uint8.result);
}

TEST("hash::fnv hashes bool values") {
  caf::hash::fnv<uint32_t> f_true;
  f_true.value(true);
  caf::hash::fnv<uint32_t> f_false;
  f_false.value(false);
  check_ne(f_true.result, f_false.result);
}

TEST("hash::fnv hashes float and double deterministically") {
  using hash_type = caf::hash::fnv<uint32_t>;
  auto h1 = hash_type::compute(1.0f);
  auto h2 = hash_type::compute(1.0f);
  check_eq(h1, h2);
  auto h3 = hash_type::compute(1.0);
  auto h4 = hash_type::compute(1.0);
  check_eq(h3, h4);
}

TEST("hash::fnv hashes byte spans") {
  using hash_type = caf::hash::fnv<uint32_t>;
  auto bytes = std::array<std::byte, 4>{
    {std::byte{'a'}, std::byte{'b'}, std::byte{'c'}, std::byte{'d'}}};
  check_eq(hash_type::compute("abcd"s),
           hash_type::compute(caf::const_byte_span(bytes)));
}

TEST("hash::fnv error state can be set and read") {
  caf::hash::fnv<uint32_t> f;
  check(f.get_error().empty());
  f.set_error(caf::make_error(caf::sec::runtime_error, "test error"));
  check(!f.get_error().empty());
}

TEST("hash::fnv hashes optional values") {
  using hash_type = caf::hash::fnv<uint32_t>;
  auto with_value = hash_type::compute(std::optional<int>{42});
  auto empty = hash_type::compute(std::optional<int>{});
  check_ne(with_value, empty);
}

TEST("hash::fnv hashes variant values") {
  using hash_type = caf::hash::fnv<uint32_t>;
  auto as_int = hash_type::compute(std::variant<int, std::string>{42});
  auto as_string
    = hash_type::compute(std::variant<int, std::string>{std::string{"42"}});
  check_ne(as_int, as_string);
}

TEST("hash::fnv hashes optional variant") {
  using hash_type = caf::hash::fnv<uint32_t>;
  auto present
    = hash_type::compute(std::optional<std::variant<int, std::string>>{
      std::variant<int, std::string>{1}});
  auto absent
    = hash_type::compute(std::optional<std::variant<int, std::string>>{});
  check_ne(present, absent);
}

TEST("fnv::compute generates a hash value in one step") {
  SECTION("32-bit") {
    using hash_type = caf::hash::fnv<uint32_t>;
    check_eq(hash_type::compute(), 0x811C9DC5u);
    check_eq(hash_type::compute("abcd"s), 0xCE3479BDu);
    check_eq(hash_type::compute("C++ Actor Framework"s), 0x2FF91FE5u);
  }
  SECTION("64-bit") {
    using hash_type = caf::hash::fnv<uint64_t>;
    check_eq(hash_type::compute(), 0xCBF29CE484222325ull);
    check_eq(hash_type::compute("abcd"s), 0xFC179F83EE0724DDull);
    check_eq(hash_type::compute("C++ Actor Framework"s), 0xA229A760C3AF69C5ull);
  }
  SECTION("size_t matches word size") {
    using hash_type = caf::hash::fnv<size_t>;
    if constexpr (sizeof(size_t) == 4) {
      check_eq(hash_type::compute(), 0x811C9DC5u);
      check_eq(hash_type::compute("abcd"s), 0xCE3479BDu);
    } else {
      check_eq(hash_type::compute(), 0xCBF29CE484222325ull);
      check_eq(hash_type::compute("abcd"s), 0xFC179F83EE0724DDull);
    }
  }
}

namespace {

struct foo {
  std::string bar;
};

template <class Inspector>
bool inspect(Inspector& f, foo& x) {
  return f.object(x).fields(f.field("bar", x.bar));
}

} // namespace

TEST("fnv::compute can create hash values for types with inspect overloads") {
  using hash_type = caf::hash::fnv<uint32_t>;
  check_eq(hash_type::compute(foo{"abcd"}), 0xCE3479BDu);
  check_eq(hash_type::compute(foo{"C++ Actor Framework"}), 0x2FF91FE5u);
}
