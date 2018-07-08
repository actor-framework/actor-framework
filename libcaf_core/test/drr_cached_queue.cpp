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

#define CAF_SUITE drr_cached_queue

#include <memory>

#include "caf/test/unit_test.hpp"

#include "caf/intrusive/singly_linked.hpp"
#include "caf/intrusive/drr_cached_queue.hpp"

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

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(drr_cached_queue_tests, fixture)

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
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(fseq, "246");
  CAF_CHECK_EQUAL(queue.deficit(), 0);
  // Allow g to consume 1, 3, 5, and 7.
  round_result = queue.new_round(4, g);
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(gseq, "1357");
  CAF_CHECK_EQUAL(queue.deficit(), 0);
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
  CAF_MESSAGE("make a round on an empty queue");
  CAF_CHECK_EQUAL(queue.new_round(10, f), make_new_round_result(false));
  CAF_MESSAGE("make a round on a queue with only odd numbers (skip all)");
  fill(queue, 1, 3, 5);
  CAF_CHECK_EQUAL(queue.new_round(10, f), make_new_round_result(false));
  CAF_MESSAGE("make a round on a queue with an even number at the front");
  fill(queue, 2);
  CAF_CHECK_EQUAL(queue.new_round(10, f), make_new_round_result(true));
  CAF_CHECK_EQUAL(seq, "2");
  CAF_MESSAGE("make a round on a queue with an even number in between");
  fill(queue, 7, 9, 4, 11, 13);
  CAF_CHECK_EQUAL(queue.new_round(10, f), make_new_round_result(true));
  CAF_CHECK_EQUAL(seq, "24");
  CAF_MESSAGE("make a round on a queue with an even number at the back");
  fill(queue, 15, 17, 6);
  CAF_CHECK_EQUAL(queue.new_round(10, f), make_new_round_result(true));
  CAF_CHECK_EQUAL(seq, "246");
}

CAF_TEST(take_front) {
  std::string seq;
  fill(queue, 1, 2, 3, 4, 5, 6);
  auto f = [&](inode& x) {
    seq += to_string(x);
    return task_result::resume;
  };
  CAF_CHECK_EQUAL(queue.deficit(), 0);
  while (!queue.empty()) {
    auto ptr = queue.take_front();
    f(*ptr);
  }
  CAF_CHECK_EQUAL(seq, "123456");
  fill(queue, 5, 4, 3, 2, 1);
  while (!queue.empty()) {
    auto ptr = queue.take_front();
    f(*ptr);
  }
  CAF_CHECK_EQUAL(seq, "12345654321");
  CAF_CHECK_EQUAL(queue.deficit(), 0);
}

CAF_TEST(alternating_consumer) {
  using fun_type = std::function<task_result (inode&)>;
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
  auto h = [&](inode& x) {
    return (*selected)(x);
  };
  // Fill and consume queue, h leaves 9 in the cache since it reads (odd, even)
  // sequences and no odd value to read after 7 is available.
  fill(queue, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  auto round_result = queue.new_round(1000, h);
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(seq, "21436587");
  CAF_CHECK_EQUAL(queue.deficit(), 0);
  CAF_CHECK_EQUAL(deep_to_string(queue.cache()), "[9]");
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
  queue.emplace_back(2);
  CAF_CHECK_EQUAL(queue_to_string(), "2");
  queue.cache().emplace_back(1);
  CAF_CHECK_EQUAL(queue_to_string(), "2");
  queue.emplace_back(3);
  CAF_CHECK_EQUAL(queue_to_string(), "2, 3");
  queue.flush_cache();
  CAF_CHECK_EQUAL(queue_to_string(), "1, 2, 3");
}

CAF_TEST(to_string) {
  CAF_CHECK_EQUAL(deep_to_string(queue), "[]");
  fill(queue, 3, 4);
  CAF_CHECK_EQUAL(deep_to_string(queue), "[3, 4]");
  fill(queue.cache(), 1, 2);
  CAF_CHECK_EQUAL(deep_to_string(queue), "[3, 4]");
  queue.flush_cache();
  CAF_CHECK_EQUAL(deep_to_string(queue), "[1, 2, 3, 4]");
}


CAF_TEST_FIXTURE_SCOPE_END()
