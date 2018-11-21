/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE ringbuffer

#include "caf/detail/ringbuffer.hpp"

#include "caf/test/dsl.hpp"

#include <algorithm>

using namespace caf;

namespace {

static constexpr size_t buf_size = 64;

using int_ringbuffer = detail::ringbuffer<int, buf_size>;

std::vector<int> consumer(int_ringbuffer& buf, size_t num) {
  std::vector<int> result;
  for (size_t i = 0; i < num; ++i) {
    buf.wait_nonempty();
    result.emplace_back(buf.front());
    buf.pop_front();
  }
  return result;
}

void producer(int_ringbuffer& buf, int first, int last) {
  for (auto i = first; i != last; ++i)
    buf.push_back(std::move(i));
}

struct fixture {
  int_ringbuffer buf;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(ringbuffer_tests, fixture)

CAF_TEST(construction) {
  CAF_CHECK_EQUAL(buf.empty(), true);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 0u);
}

CAF_TEST(push_back) {
  CAF_MESSAGE("add one element");
  buf.push_back(42);
  CAF_CHECK_EQUAL(buf.empty(), false);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 1u);
  CAF_CHECK_EQUAL(buf.front(), 42);
  CAF_MESSAGE("remove element");
  buf.pop_front();
  CAF_CHECK_EQUAL(buf.empty(), true);
  CAF_CHECK_EQUAL(buf.full(), false);
  CAF_CHECK_EQUAL(buf.size(), 0u);
  CAF_MESSAGE("fill buffer");
  for (int i = 0; i < static_cast<int>(buf_size - 1); ++i)
    buf.push_back(std::move(i));
  CAF_CHECK_EQUAL(buf.empty(), false);
  CAF_CHECK_EQUAL(buf.full(), true);
  CAF_CHECK_EQUAL(buf.size(), buf_size - 1);
  CAF_CHECK_EQUAL(buf.front(), 0);
}

CAF_TEST(concurrent access) {
  std::vector<std::thread> producers;
  producers.emplace_back(producer, std::ref(buf), 0, 100);
  producers.emplace_back(producer, std::ref(buf), 100, 200);
  producers.emplace_back(producer, std::ref(buf), 200, 300);
  auto vec = consumer(buf, 300);
  std::sort(vec.begin(), vec.end());
  CAF_CHECK(std::is_sorted(vec.begin(), vec.end()));
  CAF_CHECK_EQUAL(vec.size(), 300u);
  CAF_CHECK_EQUAL(vec.front(), 0);
  CAF_CHECK_EQUAL(vec.back(), 299);
  for (auto& t : producers)
    t.join();
}

CAF_TEST_FIXTURE_SCOPE_END()
