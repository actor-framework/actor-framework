// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE hash.fnv

#include "caf/hash/fnv.hpp"

#include "core-test.hpp"

#include <string>

using namespace caf;

using namespace std::string_literals;

template <class... Ts>
auto fnv32_hash(Ts&&... xs) {
  return hash::fnv<uint32_t>::compute(std::forward<Ts>(xs)...);
}

template <class... Ts>
auto fnv64_hash(Ts&&... xs) {
  return hash::fnv<uint64_t>::compute(std::forward<Ts>(xs)...);
}

CAF_TEST(FNV hashes build incrementally) {
  hash::fnv<uint32_t> f;
  CHECK_EQ(f.result, 0x811C9DC5u);
  f.value('a');
  CHECK_EQ(f.result, 0xE40C292Cu);
  f.value('b');
  CHECK_EQ(f.result, 0x4D2505CAu);
  f.value('c');
  CHECK_EQ(f.result, 0x1A47E90Bu);
  f.value('d');
  CHECK_EQ(f.result, 0xCE3479BDu);
}

CAF_TEST(FNV supports uint32 hashing) {
  CHECK_EQ(fnv32_hash(), 0x811C9DC5u);
  CHECK_EQ(fnv32_hash("abcd"s), 0xCE3479BDu);
  CHECK_EQ(fnv32_hash("C++ Actor Framework"s), 0x2FF91FE5u);
}

CAF_TEST(FNV supports uint64 hashing) {
  CHECK_EQ(fnv64_hash(), 0xCBF29CE484222325ull);
  CHECK_EQ(fnv64_hash("abcd"s), 0xFC179F83EE0724DDull);
  CHECK_EQ(fnv64_hash("C++ Actor Framework"s), 0xA229A760C3AF69C5ull);
}
