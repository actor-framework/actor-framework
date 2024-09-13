// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/lp/frame.hpp"

#include "caf/test/test.hpp"

using namespace caf;

namespace {

template <class... Ts>
std::vector<std::byte> to_byte_buf(Ts... values) {
  return std::vector<std::byte>{static_cast<std::byte>(values)...};
}

template <class T>
std::vector<std::byte> to_vec(span<const T> values) {
  return std::vector<std::byte>{values.begin(), values.end()};
}

} // namespace

TEST("default construction") {
  net::lp::frame uut;
  check(!uut);
  check(uut.empty());
  check(uut.bytes().empty());
  check_eq(uut.size(), 0u);
}

TEST("construction from a single buffer") {
  auto buf = to_byte_buf(1, 2, 3);
  auto uut = net::lp::frame{make_span(buf)};
  check(static_cast<bool>(uut));
  check(!uut.empty());
  check(!uut.bytes().empty());
  check_eq(uut.size(), 3u);
  check_eq(uut.bytes().size(), 3u);
  check_eq(to_vec(uut.bytes()), buf);
}

TEST("construction from multiple buffers") {
  auto buf1 = to_byte_buf(1, 2);
  auto buf2 = to_byte_buf();
  auto buf3 = to_byte_buf(3, 4, 5);
  auto uut = net::lp::frame::from_buffers(buf1, buf2, buf3);
  check(static_cast<bool>(uut));
  check(!uut.empty());
  check(!uut.bytes().empty());
  check_eq(uut.size(), 5u);
  check_eq(uut.bytes().size(), 5u);
  check_eq(to_vec(uut.bytes()), to_byte_buf(1, 2, 3, 4, 5));
}

TEST("copying, moving and swapping") {
  auto buf = to_byte_buf(1, 2, 3);
  auto uut1 = net::lp::frame{};
  auto uut2 = net::lp::frame{make_span(buf)};
  auto uut3 = uut1;
  auto uut4 = uut2;
  check_eq(uut1.bytes().data(), uut3.bytes().data());
  check_eq(uut2.bytes().data(), uut4.bytes().data());
  check_ne(uut1.bytes().data(), uut2.bytes().data());
  auto uut5 = std::move(uut1);
  auto uut6 = std::move(uut2);
  check_eq(uut5.bytes().data(), uut3.bytes().data());
  check_eq(uut6.bytes().data(), uut4.bytes().data());
  check_ne(uut5.bytes().data(), uut6.bytes().data());
  uut5.swap(uut6);
  check_eq(uut6.bytes().data(), uut3.bytes().data());
  check_eq(uut5.bytes().data(), uut4.bytes().data());
}
