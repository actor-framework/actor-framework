// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/receive_buffer.hpp"

#include "caf/test/test.hpp"

#include <algorithm>
#include <string_view>

using namespace std::literals;

using caf::io::network::receive_buffer;

SUITE("io.network.receive_buffer") {

TEST("construction") {
  SECTION("default-constructed buffers are empty") {
    receive_buffer uut;
    check_eq(uut.size(), 0u);
    check_eq(uut.capacity(), 0u);
    check(uut.data() == nullptr);
    check(uut.empty());
  }
  SECTION("constructing with a size > 0 allocates memory") {
    receive_buffer uut(1024);
    check_eq(uut.size(), 1024u);
    check_eq(uut.capacity(), 1024u);
    check(uut.data() != nullptr);
    check(!uut.empty());
  }
  SECTION("move-constructing from a buffer transfers ownership") {
    receive_buffer src(1024);
    receive_buffer uut{std::move(src)};
    check_eq(uut.size(), 1024u);
    check_eq(uut.capacity(), 1024u);
    check(uut.data() != nullptr);
    check(!uut.empty());
  }
}

TEST("reserve allocates memory if necessary") {
  SECTION("reserve(0) is a no-op") {
    receive_buffer uut;
    uut.reserve(0);
    check_eq(uut.size(), 0u);
    check_eq(uut.capacity(), 0u);
    check(uut.data() == nullptr);
    check(uut.empty());
  }
  SECTION("reserve(n) allocates memory if n > capacity") {
    receive_buffer uut;
    uut.reserve(1024);
    check_eq(uut.size(), 0u);
    check_eq(uut.capacity(), 1024u);
    check(uut.data() != nullptr);
    check(uut.begin() == uut.end());
    check(uut.empty());
  }
  SECTION("reserve(n) is a no-op if n <= capacity") {
    receive_buffer uut;
    uut.reserve(1024);
    check_eq(uut.size(), 0u);
    check_eq(uut.capacity(), 1024u);
    check(uut.data() != nullptr);
    check(uut.begin() == uut.end());
    check(uut.empty());
    auto data = uut.data();
    uut.reserve(512);
    check_eq(uut.size(), 0u);
    check_eq(uut.capacity(), 1024u);
    check(uut.data() == data);
  }
}

TEST("resize adds or removes elements if necessary") {
  SECTION("resize(0) is a no-op") {
    receive_buffer uut;
    uut.resize(0);
    check_eq(uut.size(), 0u);
    check_eq(uut.capacity(), 0u);
    check(uut.empty());
  }
  SECTION("resize(n) is a no-op if n == size") {
    receive_buffer uut(1024);
    check_eq(uut.size(), 1024u);
    uut.resize(1024);
    check_eq(uut.size(), 1024u);
    check_eq(uut.capacity(), 1024u);
    check(uut.data() != nullptr);
    check(!uut.empty());
  }
  SECTION("resize(n) adds elements if n > size") {
    receive_buffer uut;
    uut.resize(1024);
    check_eq(uut.size(), 1024u);
    check_eq(uut.capacity(), 1024u);
    check(uut.data() != nullptr);
    check(!uut.empty());
  }
  SECTION("resize(n) removes elements if n < size") {
    receive_buffer uut(1024);
    check_eq(uut.size(), 1024u);
    uut.resize(512);
    check_eq(uut.size(), 512u);
    check_eq(uut.capacity(), 1024u);
    check(uut.data() != nullptr);
    check(!uut.empty());
  }
}

TEST("clear removes all elements") {
  receive_buffer uut(1024);
  check_eq(uut.size(), 1024u);
  uut.clear();
  check_eq(uut.size(), 0u);
  check_eq(uut.capacity(), 1024u);
  check(uut.data() != nullptr);
  check(uut.empty());
}

TEST("push_back appends elements") {
  receive_buffer uut;
  uut.push_back('h');
  uut.push_back('e');
  uut.push_back('l');
  uut.push_back('l');
  uut.push_back('o');
  check_eq(uut.size(), 5u);
  check_eq(uut.capacity(), 8u);
  check(uut.data() != nullptr);
  check(!uut.empty());
  check_eq(std::string_view(uut.data(), uut.size()), "hello");
}

TEST("insert adds elements at the given position") {
  receive_buffer uut;
  uut.insert(uut.begin(), 'o');
  uut.insert(uut.begin(), 'l');
  uut.insert(uut.begin(), 'l');
  uut.insert(uut.begin(), 'e');
  uut.insert(uut.begin(), 'h');
  check_eq(std::string_view(uut.data(), uut.size()), "hello");
  auto world = "world"sv;
  uut.insert(uut.end(), world.begin(), world.end());
  uut.insert(uut.begin() + 5, ' ');
  check_eq(std::string_view(uut.data(), uut.size()), "hello world");
}

TEST("shrink_to_fit reduces capacity to size") {
  auto str = "hello world"sv;
  receive_buffer uut;
  uut.reserve(512);
  check_eq(uut.capacity(), 512u);
  uut.insert(uut.end(), str.begin(), str.end());
  uut.shrink_to_fit();
  check_eq(uut.capacity(), str.size());
}

TEST("swap exchanges the content of two buffers") {
  auto str1 = "hello"sv;
  auto str2 = "world"sv;
  receive_buffer buf1;
  receive_buffer buf2;
  buf1.insert(buf1.end(), str1.begin(), str1.end());
  buf2.insert(buf2.end(), str2.begin(), str2.end());
  auto* buf1_data = buf1.data();
  auto* buf2_data = buf2.data();
  check_eq(std::string_view(buf1.data(), buf1.size()), "hello");
  check_eq(std::string_view(buf2.data(), buf2.size()), "world");
  buf1.swap(buf2);
  check_eq(std::string_view(buf1.data(), buf1.size()), "world");
  check_eq(std::string_view(buf2.data(), buf2.size()), "hello");
  check(buf1.data() == buf2_data);
  check(buf2.data() == buf1_data);
}

} // SUITE("io.network.receive_buffer")
