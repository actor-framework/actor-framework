// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/web_socket/frame.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace std::literals;

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
  net::web_socket::frame uut;
  check(!uut);
  check(!uut.is_binary());
  check(!uut.is_text());
  check(uut.empty());
  check_eq(uut.size(), 0u);
}

TEST("construction from a single byte buffer") {
  auto buf = to_byte_buf(1, 2, 3);
  auto uut = net::web_socket::frame{make_span(buf)};
  check(static_cast<bool>(uut));
  check(!uut.empty());
  check_eq(uut.size(), 3u);
  if (uut.is_binary()) {
    auto bytes = uut.as_binary();
    check_eq(bytes.size(), 3u);
    check_eq(to_vec(bytes), buf);
  }
}

TEST("construction from multiple byte buffers") {
  auto buf1 = to_byte_buf(1, 2);
  auto buf2 = to_byte_buf();
  auto buf3 = to_byte_buf(3, 4, 5);
  auto buf4 = to_byte_buf(1, 2, 3, 4, 5);
  auto uut = net::web_socket::frame::from_buffers(buf1, buf2, buf3);
  check(static_cast<bool>(uut));
  check(!uut.empty());
  check_eq(uut.size(), 5u);
  if (uut.is_binary()) {
    auto bytes = uut.as_binary();
    check_eq(bytes.size(), 5u);
    check_eq(to_vec(bytes), buf4);
  }
}

TEST("construction from a single text buffer") {
  auto uut = net::web_socket::frame{"foo"sv};
  check(static_cast<bool>(uut));
  check(!uut.empty());
  check_eq(uut.size(), 3u);
  if (check(uut.is_text())) {
    check_eq(uut.as_text(), "foo"sv);
  }
}

TEST("construction from multiple text buffers") {
  auto buf1 = "foo"sv;
  auto buf2 = ""sv;
  auto buf3 = "bar"sv;
  auto buf4 = "foobar"sv;
  auto uut = net::web_socket::frame::from_strings(buf1, buf2, buf3);
  check(static_cast<bool>(uut));
  check(!uut.empty());
  check_eq(uut.size(), 6u);
  if (uut.is_text()) {
    check_eq(uut.as_text(), buf4);
  }
}

TEST("copying, moving and swapping") {
  auto buf = to_byte_buf(1, 2, 3);
  auto uut1 = net::web_socket::frame{};
  auto uut2 = net::web_socket::frame{make_span(buf)};
  auto uut3 = uut1;
  auto uut4 = uut2;
  check_eq(uut1.as_binary().data(), uut3.as_binary().data());
  check_eq(uut2.as_binary().data(), uut4.as_binary().data());
  check_ne(uut1.as_binary().data(), uut2.as_binary().data());
  auto uut5 = std::move(uut1);
  auto uut6 = std::move(uut2);
  check_eq(uut5.as_binary().data(), uut3.as_binary().data());
  check_eq(uut6.as_binary().data(), uut4.as_binary().data());
  check_ne(uut5.as_binary().data(), uut6.as_binary().data());
  uut5.swap(uut6);
  check_eq(uut6.as_binary().data(), uut3.as_binary().data());
  check_eq(uut5.as_binary().data(), uut4.as_binary().data());
}
