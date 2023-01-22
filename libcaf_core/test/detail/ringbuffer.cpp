// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.ringbuffer

#include "caf/detail/ringbuffer.hpp"

#include "core-test.hpp"

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

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(construction) {
  CHECK_EQ(buf.empty(), true);
  CHECK_EQ(buf.full(), false);
  CHECK_EQ(buf.size(), 0u);
}

CAF_TEST(push_back) {
  MESSAGE("add one element");
  buf.push_back(42);
  CHECK_EQ(buf.empty(), false);
  CHECK_EQ(buf.full(), false);
  CHECK_EQ(buf.size(), 1u);
  CHECK_EQ(buf.front(), 42);
  MESSAGE("remove element");
  buf.pop_front();
  CHECK_EQ(buf.empty(), true);
  CHECK_EQ(buf.full(), false);
  CHECK_EQ(buf.size(), 0u);
  MESSAGE("fill buffer");
  for (int i = 0; i < static_cast<int>(buf_size - 1); ++i)
    buf.push_back(std::move(i));
  CHECK_EQ(buf.empty(), false);
  CHECK_EQ(buf.full(), true);
  CHECK_EQ(buf.size(), buf_size - 1);
  CHECK_EQ(buf.front(), 0);
}

CAF_TEST(get all) {
  using array_type = std::array<int, buf_size>;
  using vector_type = std::vector<int>;
  array_type tmp;
  auto fetch_all = [&] {
    auto i = tmp.begin();
    auto e = buf.get_all(i);
    return vector_type(i, e);
  };
  MESSAGE("add five element");
  for (int i = 0; i < 5; ++i)
    buf.push_back(std::move(i));
  CHECK_EQ(buf.empty(), false);
  CHECK_EQ(buf.full(), false);
  CHECK_EQ(buf.size(), 5u);
  CHECK_EQ(buf.front(), 0);
  MESSAGE("drain elements");
  CHECK_EQ(fetch_all(), vector_type({0, 1, 2, 3, 4}));
  CHECK_EQ(buf.empty(), true);
  CHECK_EQ(buf.full(), false);
  CHECK_EQ(buf.size(), 0u);
  MESSAGE("add 60 elements (wraps around)");
  vector_type expected;
  for (int i = 0; i < 60; ++i) {
    expected.push_back(i);
    buf.push_back(std::move(i));
  }
  CHECK_EQ(buf.size(), 60u);
  CHECK_EQ(fetch_all(), expected);
  CHECK_EQ(buf.empty(), true);
  CHECK_EQ(buf.full(), false);
  CHECK_EQ(buf.size(), 0u);
}

CAF_TEST(concurrent access) {
  std::vector<std::thread> producers;
  producers.emplace_back(producer, std::ref(buf), 0, 100);
  producers.emplace_back(producer, std::ref(buf), 100, 200);
  producers.emplace_back(producer, std::ref(buf), 200, 300);
  auto vec = consumer(buf, 300);
  std::sort(vec.begin(), vec.end());
  CHECK(std::is_sorted(vec.begin(), vec.end()));
  CHECK_EQ(vec.size(), 300u);
  CHECK_EQ(vec.front(), 0);
  CHECK_EQ(vec.back(), 299);
  for (auto& t : producers)
    t.join();
}

END_FIXTURE_SCOPE()
