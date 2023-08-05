// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE intrusive.drr_cached_queue

#include "caf/intrusive/drr_cached_queue.hpp"

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

std::string to_string(const inode& x) {
  return std::to_string(x.value);
}

struct inode_policy {
  using mapped_type = inode;

  using task_size_type = int;

  using deficit_type = int;

  using deleter_type = std::default_delete<mapped_type>;

  using unique_pointer = std::unique_ptr<mapped_type, deleter_type>;

  static inline task_size_type task_size(const mapped_type&) noexcept {
    return 1;
  }
};

using queue_type = drr_cached_queue<inode_policy>;

struct fixture {
  inode_policy policy;
  queue_type queue{policy};

  template <class Queue>
  void fill(Queue&) {
    // nop
  }

  template <class Queue, class T, class... Ts>
  void fill(Queue& q, T x, Ts... xs) {
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
}

CAF_TEST(new_round) {
  // Define a function object for consuming even numbers.
  std::string fseq;
  auto f = [&](inode& x) -> task_result {
    if ((x.value & 0x01) == 1)
      return task_result::skip;
    fseq += to_string(x);
    return task_result::resume;
  };
  // Define a function object for consuming odd numbers.
  std::string gseq;
  auto g = [&](inode& x) -> task_result {
    if ((x.value & 0x01) == 0)
      return task_result::skip;
    gseq += to_string(x);
    return task_result::resume;
  };
  fill(queue, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  // Allow f to consume 2, 4, and 6.
  auto round_result = queue.new_round(3, f);
  CHECK_EQ(round_result, make_new_round_result(3, false));
  CHECK_EQ(fseq, "246");
  CHECK_EQ(queue.deficit(), 0);
  // Allow g to consume 1, 3, 5, and 7.
  round_result = queue.new_round(4, g);
  CHECK_EQ(round_result, make_new_round_result(4, false));
  CHECK_EQ(gseq, "1357");
  CHECK_EQ(queue.deficit(), 0);
}

CAF_TEST(skipping) {
  // Define a function object for consuming even numbers.
  std::string seq;
  auto f = [&](inode& x) -> task_result {
    if ((x.value & 0x01) == 1)
      return task_result::skip;
    seq += to_string(x);
    return task_result::resume;
  };
  MESSAGE("make a round on an empty queue");
  CHECK_EQ(queue.new_round(10, f), make_new_round_result(0, false));
  MESSAGE("make a round on a queue with only odd numbers (skip all)");
  fill(queue, 1, 3, 5);
  CHECK_EQ(queue.new_round(10, f), make_new_round_result(0, false));
  MESSAGE("make a round on a queue with an even number at the front");
  fill(queue, 2);
  CHECK_EQ(queue.new_round(10, f), make_new_round_result(1, false));
  CHECK_EQ(seq, "2");
  MESSAGE("make a round on a queue with an even number in between");
  fill(queue, 7, 9, 4, 11, 13);
  CHECK_EQ(queue.new_round(10, f), make_new_round_result(1, false));
  CHECK_EQ(seq, "24");
  MESSAGE("make a round on a queue with an even number at the back");
  fill(queue, 15, 17, 6);
  CHECK_EQ(queue.new_round(10, f), make_new_round_result(1, false));
  CHECK_EQ(seq, "246");
}

CAF_TEST(take_front) {
  std::string seq;
  fill(queue, 1, 2, 3, 4, 5, 6);
  auto f = [&](inode& x) {
    seq += to_string(x);
    return task_result::resume;
  };
  CHECK_EQ(queue.deficit(), 0);
  while (!queue.empty()) {
    auto ptr = queue.take_front();
    f(*ptr);
  }
  CHECK_EQ(seq, "123456");
  fill(queue, 5, 4, 3, 2, 1);
  while (!queue.empty()) {
    auto ptr = queue.take_front();
    f(*ptr);
  }
  CHECK_EQ(seq, "12345654321");
  CHECK_EQ(queue.deficit(), 0);
}

CAF_TEST(alternating_consumer) {
  using fun_type = std::function<task_result(inode&)>;
  fun_type f;
  fun_type g;
  fun_type* selected = &f;
  // Define a function object for consuming even numbers.
  std::string seq;
  f = [&](inode& x) -> task_result {
    if ((x.value & 0x01) == 1)
      return task_result::skip;
    seq += to_string(x);
    selected = &g;
    return task_result::resume;
  };
  // Define a function object for consuming odd numbers.
  g = [&](inode& x) -> task_result {
    if ((x.value & 0x01) == 0)
      return task_result::skip;
    seq += to_string(x);
    selected = &f;
    return task_result::resume;
  };
  /// Define a function object that alternates between f and g.
  auto h = [&](inode& x) { return (*selected)(x); };
  // Fill and consume queue, h leaves 9 in the cache since it reads (odd, even)
  // sequences and no odd value to read after 7 is available.
  fill(queue, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  auto round_result = queue.new_round(1000, h);
  CHECK_EQ(round_result, make_new_round_result(8, false));
  CHECK_EQ(seq, "21436587");
  CHECK_EQ(queue.deficit(), 0);
  CHECK_EQ(deep_to_string(queue.cache()), "[9]");
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
  queue.emplace_back(2);
  CHECK_EQ(queue_to_string(), "2");
  queue.cache().emplace_back(1);
  CHECK_EQ(queue_to_string(), "2");
  queue.emplace_back(3);
  CHECK_EQ(queue_to_string(), "2, 3");
  queue.flush_cache();
  CHECK_EQ(queue_to_string(), "1, 2, 3");
}

CAF_TEST(to_string) {
  CHECK_EQ(deep_to_string(queue.items()), "[]");
  fill(queue, 3, 4);
  CHECK_EQ(deep_to_string(queue.items()), "[3, 4]");
  fill(queue.cache(), 1, 2);
  CHECK_EQ(deep_to_string(queue.items()), "[3, 4]");
  queue.flush_cache();
  CHECK_EQ(deep_to_string(queue.items()), "[1, 2, 3, 4]");
}

END_FIXTURE_SCOPE()
