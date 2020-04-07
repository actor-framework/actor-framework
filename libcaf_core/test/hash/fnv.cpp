/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE hash.fnv

#include "caf/hash/fnv.hpp"

#include "caf/test/dsl.hpp"

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
  CAF_CHECK_EQUAL(f.value, 0x811C9DC5u);
  f('a');
  CAF_CHECK_EQUAL(f.value, 0xE40C292Cu);
  f('b');
  CAF_CHECK_EQUAL(f.value, 0x4D2505CAu);
  f('c');
  CAF_CHECK_EQUAL(f.value, 0x1A47E90Bu);
  f('d');
  CAF_CHECK_EQUAL(f.value, 0xCE3479BDu);
}

CAF_TEST(FNV supports uint32 hashing) {
  CAF_CHECK_EQUAL(fnv32_hash(), 0x811C9DC5u);
  CAF_CHECK_EQUAL(fnv32_hash("abcd"s), 0xCE3479BDu);
  CAF_CHECK_EQUAL(fnv32_hash("C++ Actor Framework"s), 0x2FF91FE5u);
}

CAF_TEST(FNV supports uint64 hashing) {
  CAF_CHECK_EQUAL(fnv64_hash(), 0xCBF29CE484222325ull);
  CAF_CHECK_EQUAL(fnv64_hash("abcd"s), 0xFC179F83EE0724DDull);
  CAF_CHECK_EQUAL(fnv64_hash("C++ Actor Framework"s), 0xA229A760C3AF69C5ull);
}
