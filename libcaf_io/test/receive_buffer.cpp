/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE io_receive_buffer
#include "caf/test/unit_test.hpp"

#include <algorithm>

#include "caf/io/network/receive_buffer.hpp"

using namespace caf;
using caf::io::network::receive_buffer;

struct fixture {

receive_buffer a;
receive_buffer b;
std::vector<char> vec;

fixture()
  : a{},
    b{1024ul},
    vec{'h', 'a', 'l', 'l', 'o'} {
  // nop
}

};


CAF_TEST_FIXTURE_SCOPE(receive_buffer_tests, fixture)

CAF_TEST(constuctors) {
  CAF_CHECK_EQUAL(a.size(), 0ul);
  CAF_CHECK_EQUAL(a.capacity(), 0ul);
  CAF_CHECK_EQUAL(a.data(), nullptr);
  CAF_CHECK(a.empty());
  CAF_CHECK_EQUAL(b.size(), 0ul);
  CAF_CHECK_EQUAL(b.capacity(), 1024ul);
  CAF_CHECK(b.data() != nullptr);
  CAF_CHECK(b.empty());
  receive_buffer other{std::move(b)};
  CAF_CHECK_EQUAL(other.size(), 0ul);
  CAF_CHECK_EQUAL(other.capacity(), 1024ul);
  CAF_CHECK(other.data() != nullptr);
  CAF_CHECK(other.empty());
}

CAF_TEST(reserve) {
  a.reserve(0);
  CAF_CHECK_EQUAL(a.size(), 0ul);
  CAF_CHECK_EQUAL(a.capacity(), 0ul);
  CAF_CHECK(a.data() == nullptr);
  CAF_CHECK(a.empty());
  a.reserve(1024);
  CAF_CHECK_EQUAL(a.size(), 0ul);
  CAF_CHECK_EQUAL(a.capacity(), 1024ul);
  CAF_CHECK(a.data() != nullptr);
  CAF_CHECK_EQUAL(a.begin(), a.end());
  CAF_CHECK(a.empty());
  a.reserve(512);
  CAF_CHECK_EQUAL(a.size(), 0ul);
  CAF_CHECK_EQUAL(a.capacity(), 1024ul);
  CAF_CHECK(a.data() != nullptr);
  CAF_CHECK_EQUAL(a.begin(), a.end());
  CAF_CHECK(a.empty());
}

CAF_TEST(resize) {
  a.resize(512);
  CAF_CHECK_EQUAL(a.size(), 512ul);
  CAF_CHECK_EQUAL(a.capacity(), 512ul);
  CAF_CHECK(a.data() != nullptr);
  CAF_CHECK(!a.empty());
  b.resize(512);
  CAF_CHECK_EQUAL(b.size(), 512ul);
  CAF_CHECK_EQUAL(b.capacity(), 1024ul);
  CAF_CHECK(b.data() != nullptr);
  CAF_CHECK(!b.empty());
  a.resize(1024);
  std::fill(a.begin(), a.end(), 'a');
  auto cnt = 0;
  CAF_CHECK(std::all_of(a.begin(), a.end(),
                        [&](char c) {
                          ++cnt;
                          return c == 'a';
                        }));
  CAF_CHECK_EQUAL(cnt, 1024);
  a.resize(10);
  cnt = 0;
  CAF_CHECK(std::all_of(a.begin(), a.end(),
                        [&](char c) {
                          ++cnt;
                          return c == 'a';
                        }));
  CAF_CHECK_EQUAL(cnt, 10);
  a.resize(1024);
  cnt = 0;
  CAF_CHECK(std::all_of(a.begin(), a.end(),
                        [&](char c) {
                          ++cnt;
                          return c == 'a';
                        }));
  CAF_CHECK_EQUAL(cnt, 1024);
}

CAF_TEST(push_back) {
  for (auto c : vec)
    a.push_back(c);
  CAF_CHECK_EQUAL(vec.size(), a.size());
  CAF_CHECK_EQUAL(a.capacity(), 8ul);
  CAF_CHECK(a.data() != nullptr);
  CAF_CHECK(!a.empty());
  CAF_CHECK(std::equal(vec.begin(), vec.end(), a.begin()));
}

CAF_TEST(insert) {
  for (auto c: vec)
    a.insert(a.end(), c);
  CAF_CHECK_EQUAL(*(a.data() + 0), 'h');
  CAF_CHECK_EQUAL(*(a.data() + 1), 'a');
  CAF_CHECK_EQUAL(*(a.data() + 2), 'l');
  CAF_CHECK_EQUAL(*(a.data() + 3), 'l');
  CAF_CHECK_EQUAL(*(a.data() + 4), 'o');
  a.insert(a.begin(), '!');
  CAF_CHECK_EQUAL(*(a.data() + 0), '!');
  CAF_CHECK_EQUAL(*(a.data() + 1), 'h');
  CAF_CHECK_EQUAL(*(a.data() + 2), 'a');
  CAF_CHECK_EQUAL(*(a.data() + 3), 'l');
  CAF_CHECK_EQUAL(*(a.data() + 4), 'l');
  CAF_CHECK_EQUAL(*(a.data() + 5), 'o');
  a.insert(a.begin() + 4, '-');
  CAF_CHECK_EQUAL(*(a.data() + 0), '!');
  CAF_CHECK_EQUAL(*(a.data() + 1), 'h');
  CAF_CHECK_EQUAL(*(a.data() + 2), 'a');
  CAF_CHECK_EQUAL(*(a.data() + 3), 'l');
  CAF_CHECK_EQUAL(*(a.data() + 4), '-');
  CAF_CHECK_EQUAL(*(a.data() + 5), 'l');
  CAF_CHECK_EQUAL(*(a.data() + 6), 'o');
}

CAF_TEST(shrink_to_fit) {
  a.shrink_to_fit();
  CAF_CHECK_EQUAL(a.size(), 0ul);
  CAF_CHECK_EQUAL(a.capacity(), 0ul);
  CAF_CHECK(a.data() == nullptr);
  CAF_CHECK(a.empty());
}

CAF_TEST(swap) {
  for (auto c : vec)
    a.push_back(c);
  std::swap(a, b);
  CAF_CHECK_EQUAL(a.size(), 0ul);
  CAF_CHECK_EQUAL(a.capacity(), 1024ul);
  CAF_CHECK(a.data() != nullptr);
  CAF_CHECK_EQUAL(b.size(), vec.size());
  CAF_CHECK_EQUAL(std::distance(b.begin(), b.end()),
                  static_cast<receive_buffer::difference_type>(vec.size()));
  CAF_CHECK_EQUAL(b.capacity(), 8ul);
  CAF_CHECK(b.data() != nullptr);
  CAF_CHECK_EQUAL(*(b.data() + 0), 'h');
  CAF_CHECK_EQUAL(*(b.data() + 1), 'a');
  CAF_CHECK_EQUAL(*(b.data() + 2), 'l');
  CAF_CHECK_EQUAL(*(b.data() + 3), 'l');
  CAF_CHECK_EQUAL(*(b.data() + 4), 'o');
}

CAF_TEST_FIXTURE_SCOPE_END()
