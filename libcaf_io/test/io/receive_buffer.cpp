// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config.hpp"

#define CAF_SUITE io_receive_buffer
#include "io-test.hpp"

#include <algorithm>

#include "caf/io/network/receive_buffer.hpp"

using namespace caf;
using caf::io::network::receive_buffer;

namespace {

struct fixture {
  receive_buffer a;
  receive_buffer b;
  std::vector<char> vec;

  fixture() : b(1024ul), vec{'h', 'a', 'l', 'l', 'o'} {
    // nop
  }

  std::string as_string(const receive_buffer& xs) {
    std::string result;
    for (auto& x : xs)
      result += static_cast<char>(x);
    return result;
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(constructors) {
  CHECK_EQ(a.size(), 0ul);
  CHECK_EQ(a.capacity(), 0ul);
  CHECK(a.data() == nullptr);
  CHECK(a.empty());
  CHECK_EQ(b.size(), 1024ul);
  CHECK_EQ(b.capacity(), 1024ul);
  CHECK(b.data() != nullptr);
  CHECK(!b.empty());
  receive_buffer other{std::move(b)};
  CHECK_EQ(other.size(), 1024ul);
  CHECK_EQ(other.capacity(), 1024ul);
  CHECK(other.data() != nullptr);
  CHECK(!other.empty());
}

CAF_TEST(reserve) {
  a.reserve(0);
  CHECK_EQ(a.size(), 0ul);
  CHECK_EQ(a.capacity(), 0ul);
  CHECK(a.data() == nullptr);
  CHECK(a.empty());
  a.reserve(1024);
  CHECK_EQ(a.size(), 0ul);
  CHECK_EQ(a.capacity(), 1024ul);
  CHECK(a.data() != nullptr);
  CHECK(a.begin() == a.end());
  CHECK(a.empty());
  a.reserve(512);
  CHECK_EQ(a.size(), 0ul);
  CHECK_EQ(a.capacity(), 1024ul);
  CHECK(a.data() != nullptr);
  CHECK(a.begin() == a.end());
  CHECK(a.empty());
}

CAF_TEST(resize) {
  a.resize(512);
  CHECK_EQ(a.size(), 512ul);
  CHECK_EQ(a.capacity(), 512ul);
  CHECK(a.data() != nullptr);
  CHECK(!a.empty());
  b.resize(512);
  CHECK_EQ(b.size(), 512ul);
  CHECK_EQ(b.capacity(), 1024ul);
  CHECK(b.data() != nullptr);
  CHECK(!b.empty());
  a.resize(1024);
  std::fill(a.begin(), a.end(), 'a');
  auto cnt = 0;
  CHECK(std::all_of(a.begin(), a.end(), [&](char c) {
    ++cnt;
    return c == 'a';
  }));
  CHECK_EQ(cnt, 1024);
  a.resize(10);
  cnt = 0;
  CHECK(std::all_of(a.begin(), a.end(), [&](char c) {
    ++cnt;
    return c == 'a';
  }));
  CHECK_EQ(cnt, 10);
  a.resize(1024);
  cnt = 0;
  CHECK(std::all_of(a.begin(), a.end(), [&](char c) {
    ++cnt;
    return c == 'a';
  }));
  CHECK_EQ(cnt, 1024);
}

CAF_TEST(push_back) {
  for (auto c : vec)
    a.push_back(c);
  CHECK_EQ(vec.size(), a.size());
  CHECK_EQ(a.capacity(), 8ul);
  CHECK(a.data() != nullptr);
  CHECK(!a.empty());
  CHECK(std::equal(vec.begin(), vec.end(), a.begin()));
}

CAF_TEST(insert) {
  for (auto c : vec)
    a.insert(a.end(), c);
  CHECK_EQ(as_string(a), "hallo");
  a.insert(a.begin(), '!');
  CHECK_EQ(as_string(a), "!hallo");
  a.insert(a.begin() + 4, '-');
  CHECK_EQ(as_string(a), "!hal-lo");
  std::string foo = "foo:";
  a.insert(a.begin() + 1, foo.begin(), foo.end());
  CHECK_EQ(as_string(a), "!foo:hal-lo");
  std::string bar = ":bar";
  a.insert(a.end(), bar.begin(), bar.end());
  CHECK_EQ(as_string(a), "!foo:hal-lo:bar");
}

CAF_TEST(shrink_to_fit) {
  a.shrink_to_fit();
  CHECK_EQ(a.size(), 0ul);
  CHECK_EQ(a.capacity(), 0ul);
  CHECK(a.data() == nullptr);
  CHECK(a.empty());
}

CAF_TEST(swap) {
  for (auto c : vec)
    a.push_back(c);
  std::swap(a, b);
  CHECK_EQ(a.size(), 1024ul);
  CHECK_EQ(a.capacity(), 1024ul);
  CHECK(a.data() != nullptr);
  CHECK_EQ(b.size(), vec.size());
  CHECK_EQ(std::distance(b.begin(), b.end()),
           static_cast<receive_buffer::difference_type>(vec.size()));
  CHECK_EQ(b.capacity(), 8ul);
  CHECK(b.data() != nullptr);
  CHECK_EQ(*(b.data() + 0), 'h');
  CHECK_EQ(*(b.data() + 1), 'a');
  CHECK_EQ(*(b.data() + 2), 'l');
  CHECK_EQ(*(b.data() + 3), 'l');
  CHECK_EQ(*(b.data() + 4), 'o');
}

END_FIXTURE_SCOPE()
