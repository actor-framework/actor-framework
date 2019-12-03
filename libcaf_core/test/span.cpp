/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE span

#include "caf/span.hpp"

#include "caf/test/dsl.hpp"

#include <algorithm>

using namespace caf;

namespace {

using i8_list = std::vector<int8_t>;

using i16_list = std::vector<int16_t>;

template <class T, class U>
bool equal(const T& xs, const U& ys) {
  return xs.size() == ys.size() && std::equal(xs.begin(), xs.end(), ys.begin());
}

struct fixture {
  i8_list chars{'a', 'b', 'c', 'd', 'e', 'f'};

  i8_list rchars{'f', 'e', 'd', 'c', 'b', 'a'};

  i16_list shorts{1, 2, 4, 8, 16, 32, 64};

  i16_list rshorts{64, 32, 16, 8, 4, 2, 1};
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(span_tests, fixture)

CAF_TEST(default construction) {
  span<int> xs;
  CAF_CHECK_EQUAL(xs.size(), 0u);
  CAF_CHECK_EQUAL(xs.empty(), true);
  CAF_CHECK_EQUAL(xs.data(), nullptr);
  CAF_CHECK_EQUAL(xs.size_bytes(), 0u);
  CAF_CHECK_EQUAL(xs.begin(), xs.end());
  CAF_CHECK_EQUAL(xs.cbegin(), xs.cend());
  CAF_CHECK_EQUAL(xs.rbegin(), xs.rend());
  CAF_CHECK_EQUAL(xs.crbegin(), xs.crend());
  CAF_CHECK_EQUAL(as_bytes(xs).size_bytes(), 0u);
  CAF_CHECK_EQUAL(as_writable_bytes(xs).size_bytes(), 0u);
}

CAF_TEST(iterators) {
  auto xs = make_span(chars);
  CAF_CHECK(std::equal(xs.begin(), xs.end(), chars.begin()));
  CAF_CHECK(std::equal(xs.rbegin(), xs.rend(), rchars.begin()));
  auto ys = make_span(shorts);
  CAF_CHECK(std::equal(ys.begin(), ys.end(), shorts.begin()));
  CAF_CHECK(std::equal(ys.rbegin(), ys.rend(), rshorts.begin()));
}

CAF_TEST(subspans) {
  auto xs = make_span(chars);
  CAF_CHECK(equal(xs.first(6), xs));
  CAF_CHECK(equal(xs.last(6), xs));
  CAF_CHECK(equal(xs.subspan(0, 6), xs));
  CAF_CHECK(equal(xs.first(3), i8_list({'a', 'b', 'c'})));
  CAF_CHECK(equal(xs.last(3), i8_list({'d', 'e', 'f'})));
  CAF_CHECK(equal(xs.subspan(2, 2), i8_list({'c', 'd'})));
}

CAF_TEST(free iterator functions) {
  auto xs = make_span(chars);
  CAF_CHECK_EQUAL(xs.begin(), begin(xs));
  CAF_CHECK_EQUAL(xs.cbegin(), cbegin(xs));
  CAF_CHECK_EQUAL(xs.end(), end(xs));
  CAF_CHECK_EQUAL(xs.cend(), cend(xs));
}

CAF_TEST(as bytes) {
  auto xs = make_span(chars);
  auto ys = make_span(shorts);
  CAF_CHECK_EQUAL(as_bytes(xs).size(), chars.size());
  CAF_CHECK_EQUAL(as_bytes(ys).size(), shorts.size() * 2);
  CAF_CHECK_EQUAL(as_writable_bytes(xs).size(), chars.size());
  CAF_CHECK_EQUAL(as_writable_bytes(ys).size(), shorts.size() * 2);
}

CAF_TEST(make_span) {
  auto xs = make_span(chars);
  auto ys = make_span(chars.data(), chars.size());
  auto zs = make_span(chars.data(), chars.data() + chars.size());
  CAF_CHECK(std::equal(xs.begin(), xs.end(), chars.begin()));
  CAF_CHECK(std::equal(ys.begin(), ys.end(), chars.begin()));
  CAF_CHECK(std::equal(zs.begin(), zs.end(), chars.begin()));
  CAF_CHECK_EQUAL(end(xs), end(ys));
  CAF_CHECK_EQUAL(end(ys), end(zs));
  CAF_CHECK_EQUAL(begin(xs), begin(ys));
  CAF_CHECK_EQUAL(begin(ys), begin(zs));
}

CAF_TEST(spans are convertible from compatible containers) {
  std::vector<int> xs{1, 2, 3};
  span<const int> ys{xs};
  CAF_CHECK(std::equal(xs.begin(), xs.end(), ys.begin()));
}

CAF_TEST_FIXTURE_SCOPE_END()
