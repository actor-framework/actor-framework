// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE intrusive.task_queue

#include "caf/intrusive/task_queue.hpp"

#include "caf/intrusive/singly_linked.hpp"

#include "core-test.hpp"

#include <memory>

using namespace caf;
using namespace caf::intrusive;

namespace {

struct inode : singly_linked<inode> {
  int value;
  inode(int x = 0) : value(x) {
    // nop
  }
};

[[maybe_unused]] std::string to_string(const inode& x) {
  return std::to_string(x.value);
}

struct inode_policy {
  using mapped_type = inode;

  using task_size_type = int;

  using deleter_type = std::default_delete<mapped_type>;

  using unique_pointer = std::unique_ptr<mapped_type, deleter_type>;

  static inline task_size_type task_size(const mapped_type& x) {
    return x.value;
  }
};

using queue_type = task_queue<inode_policy>;

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

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(default_constructed) {
  CAF_REQUIRE_EQUAL(queue.empty(), true);
  CAF_REQUIRE_EQUAL(queue.total_task_size(), 0);
  CAF_REQUIRE_EQUAL(queue.peek(), nullptr);
  CAF_REQUIRE_EQUAL(queue.begin(), queue.end());
}

CAF_TEST(push_back) {
  queue.emplace_back(1);
  queue.push_back(inode_policy::unique_pointer{new inode(2)});
  queue.push_back(new inode(3));
  CAF_REQUIRE_EQUAL(deep_to_string(queue), "[1, 2, 3]");
}

CAF_TEST(lifo_conversion) {
  queue.lifo_append(new inode(3));
  queue.lifo_append(new inode(2));
  queue.lifo_append(new inode(1));
  queue.stop_lifo_append();
  CAF_REQUIRE_EQUAL(deep_to_string(queue), "[1, 2, 3]");
}

CAF_TEST(move_construct) {
  fill(queue, 1, 2, 3);
  queue_type q2 = std::move(queue);
  CAF_REQUIRE_EQUAL(queue.empty(), true);
  CAF_REQUIRE_EQUAL(q2.empty(), false);
  CAF_REQUIRE_EQUAL(deep_to_string(q2), "[1, 2, 3]");
}

CAF_TEST(move_assign) {
  queue_type q2{policy};
  fill(q2, 1, 2, 3);
  queue = std::move(q2);
  CAF_REQUIRE_EQUAL(q2.empty(), true);
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  CAF_REQUIRE_EQUAL(deep_to_string(queue), "[1, 2, 3]");
}

CAF_TEST(append) {
  queue_type q2{policy};
  fill(queue, 1, 2, 3);
  fill(q2, 4, 5, 6);
  queue.append(q2);
  CAF_REQUIRE_EQUAL(q2.empty(), true);
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  CAF_REQUIRE_EQUAL(deep_to_string(queue), "[1, 2, 3, 4, 5, 6]");
}

CAF_TEST(prepend) {
  queue_type q2{policy};
  fill(queue, 1, 2, 3);
  fill(q2, 4, 5, 6);
  queue.prepend(q2);
  CAF_REQUIRE_EQUAL(q2.empty(), true);
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  CAF_REQUIRE_EQUAL(deep_to_string(queue), "[4, 5, 6, 1, 2, 3]");
}

CAF_TEST(peek) {
  CHECK_EQ(queue.peek(), nullptr);
  fill(queue, 1, 2, 3);
  CHECK_EQ(queue.peek()->value, 1);
}

CAF_TEST(task_size) {
  fill(queue, 1, 2, 3);
  CHECK_EQ(queue.total_task_size(), 6);
  fill(queue, 4, 5);
  CHECK_EQ(queue.total_task_size(), 15);
  queue.clear();
  CHECK_EQ(queue.total_task_size(), 0);
}

CAF_TEST(to_string) {
  CHECK_EQ(deep_to_string(queue), "[]");
  fill(queue, 1, 2, 3, 4);
  CHECK_EQ(deep_to_string(queue), "[1, 2, 3, 4]");
}

END_FIXTURE_SCOPE()
