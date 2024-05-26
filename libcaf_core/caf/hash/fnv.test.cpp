// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/hash/fnv.hpp"

#include "caf/test/test.hpp"

#include <string>

using namespace std::literals;

namespace {

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
}

struct foo {
  std::string bar;
};

template <class Inspector>
bool inspect(Inspector& f, foo& x) {
  return f.object(x).fields(f.field("bar", x.bar));
}

TEST("fnv::compute can create hash values for types with inspect overloads") {
  using hash_type = caf::hash::fnv<uint32_t>;
  check_eq(hash_type::compute(foo{"abcd"}), 0xCE3479BDu);
  check_eq(hash_type::compute(foo{"C++ Actor Framework"}), 0x2FF91FE5u);
}

} // namespace
