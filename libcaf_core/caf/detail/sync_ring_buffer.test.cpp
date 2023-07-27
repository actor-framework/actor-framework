// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/sync_ring_buffer.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include <algorithm>
#include <vector>

using namespace caf;

namespace {

constexpr size_t int_buffer_size = 64;

using int_buffer = detail::sync_ring_buffer<int, int_buffer_size>;

std::vector<int> consumer(int_buffer& buf, size_t num) {
  std::vector<int> result;
  for (size_t i = 0; i < num; ++i) {
    buf.wait_nonempty();
    result.emplace_back(buf.front());
    buf.pop_front();
  }
  return result;
}

void producer(int_buffer& buf, int first, int last) {
  for (auto i = first; i != last; ++i)
    buf.push_back(std::move(i));
}

} // namespace

TEST("a default-constructed ring buffer is empty") {
  int_buffer buf;
  check(buf.empty());
  check(!buf.full());
  check_eq(buf.size(), 0u);
}

TEST("push_back adds one element to the ring buffer") {
  int_buffer buf;
  info("add one element");
  buf.push_back(42);
  check(!buf.empty());
  check(!buf.full());
  check_eq(buf.size(), 1u);
  check_eq(buf.front(), 42);
  info("remove element");
  buf.pop_front();
  check(buf.empty());
  check(!buf.full());
  check_eq(buf.size(), 0u);
  info("fill buffer");
  for (int i = 0; i < static_cast<int>(int_buffer_size - 1); ++i)
    buf.push_back(std::move(i));
  check(!buf.empty());
  check(buf.full());
  check_eq(buf.size(), int_buffer_size - 1);
  check_eq(buf.front(), 0);
}

TEST("get_all returns all elements from the ring buffer") {
  int_buffer buf;
  using array_type = std::array<int, int_buffer_size>;
  using vector_type = std::vector<int>;
  array_type tmp;
  auto fetch_all = [&] {
    auto i = tmp.begin();
    auto e = buf.get_all(i);
    return vector_type(i, e);
  };
  info("add five element");
  for (int i = 0; i < 5; ++i)
    buf.push_back(std::move(i));
  check(!buf.empty());
  check(!buf.full());
  check_eq(buf.size(), 5u);
  check_eq(buf.front(), 0);
  info("drain elements");
  check_eq(fetch_all(), vector_type({0, 1, 2, 3, 4}));
  check(buf.empty());
  check(!buf.full());
  check_eq(buf.size(), 0u);
  info("add 60 elements (wraps around)");
  vector_type expected;
  for (int i = 0; i < 60; ++i) {
    expected.push_back(i);
    buf.push_back(std::move(i));
  }
  check_eq(buf.size(), 60u);
  check_eq(fetch_all(), expected);
  check(buf.empty());
  check(!buf.full());
  check_eq(buf.size(), 0u);
}

TEST("sync_ring_buffer can be used with multiple producers") {
  int_buffer buf;
  std::vector<std::thread> producers;
  producers.emplace_back(producer, std::ref(buf), 0, 100);
  producers.emplace_back(producer, std::ref(buf), 100, 200);
  producers.emplace_back(producer, std::ref(buf), 200, 300);
  auto vec = consumer(buf, 300);
  std::sort(vec.begin(), vec.end());
  check(std::is_sorted(vec.begin(), vec.end()));
  check_eq(vec.size(), 300u);
  check_eq(vec.front(), 0);
  check_eq(vec.back(), 299);
  for (auto& t : producers)
    t.join();
}

CAF_TEST_MAIN()
