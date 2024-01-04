// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/span.hpp"

#include "caf/test/test.hpp"

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

WITH_FIXTURE(fixture) {

TEST("default construction") {
  span<int> xs;
  check_eq(xs.size(), 0u);
  check_eq(xs.empty(), true);
  check_eq(xs.data(), nullptr);
  check_eq(xs.size_bytes(), 0u);
  check_eq(xs.begin(), xs.end());
  check_eq(xs.cbegin(), xs.cend());
  check_eq(xs.rbegin(), xs.rend());
  check_eq(xs.crbegin(), xs.crend());
  check_eq(as_bytes(xs).size_bytes(), 0u);
  check_eq(as_writable_bytes(xs).size_bytes(), 0u);
}

TEST("iterators") {
  auto xs = make_span(chars);
  check(std::equal(xs.begin(), xs.end(), chars.begin()));
  check(std::equal(xs.rbegin(), xs.rend(), rchars.begin()));
  auto ys = make_span(shorts);
  check(std::equal(ys.begin(), ys.end(), shorts.begin()));
  check(std::equal(ys.rbegin(), ys.rend(), rshorts.begin()));
}

TEST("subspans") {
  auto xs = make_span(chars);
  check(equal(xs.first(6), xs));
  check(equal(xs.last(6), xs));
  check(equal(xs.subspan(0, 6), xs));
  check(equal(xs.first(3), i8_list({'a', 'b', 'c'})));
  check(equal(xs.last(3), i8_list({'d', 'e', 'f'})));
  check(equal(xs.subspan(2, 2), i8_list({'c', 'd'})));
}

TEST("free iterator functions") {
  auto xs = make_span(chars);
  check(xs.begin() == begin(xs));
  check(xs.cbegin() == cbegin(xs));
  check(xs.end() == end(xs));
  check(xs.cend() == cend(xs));
}

TEST("as bytes") {
  auto xs = make_span(chars);
  auto ys = make_span(shorts);
  check_eq(as_bytes(xs).size(), chars.size());
  check_eq(as_bytes(ys).size(), shorts.size() * 2);
  check_eq(as_writable_bytes(xs).size(), chars.size());
  check_eq(as_writable_bytes(ys).size(), shorts.size() * 2);
}

TEST("make_span") {
  auto xs = make_span(chars);
  auto ys = make_span(chars.data(), chars.size());
  auto zs = make_span(chars.data(), chars.data() + chars.size());
  check(std::equal(xs.begin(), xs.end(), chars.begin()));
  check(std::equal(ys.begin(), ys.end(), chars.begin()));
  check(std::equal(zs.begin(), zs.end(), chars.begin()));
  check(end(xs) == end(ys));
  check(end(ys) == end(zs));
  check(begin(xs) == begin(ys));
  check(begin(ys) == begin(zs));
}

TEST("spans are convertible from compatible containers") {
  std::vector<int> xs{1, 2, 3};
  span<const int> ys{xs};
  check(std::equal(xs.begin(), xs.end(), ys.begin()));
}

} // WITH_FIXTURE(fixture)

} // namespace
