// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE intrusive.drr_queue

#include "caf/intrusive/drr_queue.hpp"

#include "core-test.hpp"

#include <memory>

#include "caf/intrusive/singly_linked.hpp"

using namespace caf;
using namespace caf::intrusive;

namespace {

struct inode : singly_linked<inode> {
  int value;
  inode(int x = 0) : value(x) {
    // nop
  }
};

std::string to_string(const inode& x) {
  return std::to_string(x.value);
}

struct inode_policy {
  using mapped_type = inode;

  using task_size_type = int;

  using deficit_type = int;

  using deleter_type = std::default_delete<mapped_type>;

  using unique_pointer = std::unique_ptr<mapped_type, deleter_type>;

  static inline task_size_type task_size(const mapped_type& x) {
    return x.value;
  }
};

using queue_type = drr_queue<inode_policy>;

struct fixture {
  inode_policy policy;
  queue_type queue{policy};

  void fill(queue_type&) {
    // nop
  }

  template <class T, class... Ts>
  void fill(queue_type& q, T x, Ts... xs) {
    q.emplace_back(x);
    fill(q, xs...);
  }
};

auto make_new_round_result(size_t consumed_items, bool stop_all) {
  return new_round_result{consumed_items, stop_all};
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(default_constructed) {
  CAF_REQUIRE_EQUAL(queue.empty(), true);
  CAF_REQUIRE_EQUAL(queue.deficit(), 0);
  CAF_REQUIRE_EQUAL(queue.total_task_size(), 0);
  CAF_REQUIRE_EQUAL(queue.peek(), nullptr);
  CAF_REQUIRE_EQUAL(queue.next(), nullptr);
  CAF_REQUIRE_EQUAL(queue.begin(), queue.end());
}

CAF_TEST(inc_deficit) {
  // Increasing the deficit does nothing as long as the queue is empty.
  queue.inc_deficit(100);
  CAF_REQUIRE_EQUAL(queue.deficit(), 0);
  // Increasing the deficit must work on non-empty queues.
  fill(queue, 1);
  queue.inc_deficit(100);
  CAF_REQUIRE_EQUAL(queue.deficit(), 100);
  // Deficit must drop back down to 0 once the queue becomes empty.
  queue.next();
  CAF_REQUIRE_EQUAL(queue.deficit(), 0);
}

CAF_TEST(new_round) {
  std::string seq;
  fill(queue, 1, 2, 3, 4, 5, 6);
  auto f = [&](inode& x) {
    seq += to_string(x);
    return task_result::resume;
  };
  // Allow f to consume 1, 2, and 3 with a leftover deficit of 1.
  auto round_result = queue.new_round(7, f);
  CHECK_EQ(round_result, make_new_round_result(3, false));
  CHECK_EQ(seq, "123");
  CHECK_EQ(queue.deficit(), 1);
  // Allow f to consume 4 and 5 with a leftover deficit of 0.
  round_result = queue.new_round(8, f);
  CHECK_EQ(round_result, make_new_round_result(2, false));
  CHECK_EQ(seq, "12345");
  CHECK_EQ(queue.deficit(), 0);
  // Allow f to consume 6 with a leftover deficit of 0 (queue is empty).
  round_result = queue.new_round(1000, f);
  CHECK_EQ(round_result, make_new_round_result(1, false));
  CHECK_EQ(seq, "123456");
  CHECK_EQ(queue.deficit(), 0);
  // new_round on an empty queue does nothing.
  round_result = queue.new_round(1000, f);
  CHECK_EQ(round_result, make_new_round_result(0, false));
  CHECK_EQ(seq, "123456");
  CHECK_EQ(queue.deficit(), 0);
}

CAF_TEST(next) {
  std::string seq;
  fill(queue, 1, 2, 3, 4, 5, 6);
  auto f = [&](inode& x) {
    seq += to_string(x);
    return task_result::resume;
  };
  auto take = [&] {
    queue.flush_cache();
    queue.inc_deficit(queue.peek()->value);
    return queue.next();
  };
  while (!queue.empty()) {
    auto ptr = take();
    f(*ptr);
  }
  CHECK_EQ(seq, "123456");
  fill(queue, 5, 4, 3, 2, 1);
  while (!queue.empty()) {
    auto ptr = take();
    f(*ptr);
  }
  CHECK_EQ(seq, "12345654321");
  CHECK_EQ(queue.deficit(), 0);
}

CAF_TEST(peek_all) {
  auto queue_to_string = [&] {
    std::string str;
    auto peek_fun = [&](const inode& x) {
      if (!str.empty())
        str += ", ";
      str += std::to_string(x.value);
    };
    queue.peek_all(peek_fun);
    return str;
  };
  CHECK_EQ(queue_to_string(), "");
  queue.emplace_back(1);
  CHECK_EQ(queue_to_string(), "1");
  queue.emplace_back(2);
  CHECK_EQ(queue_to_string(), "1, 2");
  queue.emplace_back(3);
  CHECK_EQ(queue_to_string(), "1, 2, 3");
  queue.emplace_back(4);
  CHECK_EQ(queue_to_string(), "1, 2, 3, 4");
}

CAF_TEST(to_string) {
  CHECK_EQ(deep_to_string(queue), "[]");
  fill(queue, 1, 2, 3, 4);
  CHECK_EQ(deep_to_string(queue), "[1, 2, 3, 4]");
}

END_FIXTURE_SCOPE()
