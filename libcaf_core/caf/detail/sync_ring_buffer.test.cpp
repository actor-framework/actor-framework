// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/sync_ring_buffer.hpp"

#include "caf/test/test.hpp"

#include <algorithm>
#include <thread>
#include <vector>

using namespace caf;
using namespace std::literals;

namespace {

constexpr size_t int_buffer_size = 64;

using string_queue = detail::sync_ring_buffer<std::string, int_buffer_size>;

std::vector<int> consumer(string_queue* queue, size_t num) {
  std::vector<int> result;
  for (size_t i = 0; i < num; ++i) {
    result.emplace_back(atoi(queue->pop().c_str()));
  }
  return result;
}

void producer(string_queue* queue, int first, int last) {
  for (auto i = first; i != last; ++i)
    queue->push(std::to_string(i));
}

TEST("a default-constructed ring buffer is empty") {
  string_queue queue;
  auto now = std::chrono::steady_clock::now();
  check_eq(queue.try_pop(now), std::nullopt);
}

TEST("push adds one element to the ring buffer") {
  string_queue queue;
  queue.push("hello");
  auto now = std::chrono::steady_clock::now();
  check_eq(queue.try_pop(now), "hello"s);
  check_eq(queue.try_pop(now), std::nullopt);
}

TEST("sync_ring_buffer can be used with multiple producers") {
  string_queue queue;
  std::vector<std::thread> producers;
  // Start three producers that push 100 elements each.
  producers.emplace_back(producer, &queue, 0, 100);
  producers.emplace_back(producer, &queue, 100, 200);
  producers.emplace_back(producer, &queue, 200, 300);
  // Wait until the queue is full to his the blocking paths in push.
  while (queue.can_push())
    std::this_thread::sleep_for(1ms);
  // Consume all elements and check whether we got all of them.
  auto vec = consumer(&queue, 300);
  std::sort(vec.begin(), vec.end());
  check_eq(vec.size(), 300u);
  check_eq(vec.front(), 0);
  check_eq(vec.back(), 299);
  for (auto& t : producers)
    t.join();
}

} // namespace
