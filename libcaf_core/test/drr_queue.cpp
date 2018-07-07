/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE drr_queue

#include <memory>

#include "caf/test/unit_test.hpp"

#include "caf/intrusive/singly_linked.hpp"
#include "caf/intrusive/drr_queue.hpp"

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

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(drr_queue_tests, fixture)

CAF_TEST(default_constructed) {
  CAF_REQUIRE_EQUAL(queue.empty(), true);
  CAF_REQUIRE_EQUAL(queue.deficit(), 0);
  CAF_REQUIRE_EQUAL(queue.total_task_size(), 0);
  CAF_REQUIRE_EQUAL(queue.peek(), nullptr);
  CAF_REQUIRE_EQUAL(queue.next(), nullptr);
  CAF_REQUIRE_EQUAL(queue.begin(), queue.end());
  CAF_REQUIRE_EQUAL(queue.before_begin()->next, queue.end().ptr);
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
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(seq, "123");
  CAF_CHECK_EQUAL(queue.deficit(), 1);
  // Allow f to consume 4 and 5 with a leftover deficit of 0.
  round_result = queue.new_round(8, f);
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(seq, "12345");
  CAF_CHECK_EQUAL(queue.deficit(), 0);
  // Allow f to consume 6 with a leftover deficit of 0 (queue is empty).
  round_result = queue.new_round(1000, f);
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(seq, "123456");
  CAF_CHECK_EQUAL(queue.deficit(), 0);
  // new_round on an empty queue does nothing.
  round_result = queue.new_round(1000, f);
  CAF_CHECK_EQUAL(round_result, make_new_round_result(false));
  CAF_CHECK_EQUAL(seq, "123456");
  CAF_CHECK_EQUAL(queue.deficit(), 0);
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
  CAF_CHECK_EQUAL(seq, "123456");
  fill(queue, 5, 4, 3, 2, 1);
  while (!queue.empty()) {
    auto ptr = take();
    f(*ptr);
  }
  CAF_CHECK_EQUAL(seq, "12345654321");
  CAF_CHECK_EQUAL(queue.deficit(), 0);
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
  CAF_CHECK_EQUAL(queue_to_string(), "");
  queue.emplace_back(1);
  CAF_CHECK_EQUAL(queue_to_string(), "1");
  queue.emplace_back(2);
  CAF_CHECK_EQUAL(queue_to_string(), "1, 2");
  queue.emplace_back(3);
  CAF_CHECK_EQUAL(queue_to_string(), "1, 2, 3");
  queue.emplace_back(4);
  CAF_CHECK_EQUAL(queue_to_string(), "1, 2, 3, 4");
}

CAF_TEST(to_string) {
  CAF_CHECK_EQUAL(deep_to_string(queue), "[]");
  fill(queue, 1, 2, 3, 4);
  CAF_CHECK_EQUAL(deep_to_string(queue), "[1, 2, 3, 4]");
}

CAF_TEST_FIXTURE_SCOPE_END()
