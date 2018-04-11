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

#define CAF_SUITE wdrr_fixed_multiplexed_queue

#include <memory>

#include "caf/test/unit_test.hpp"

#include "caf/intrusive/drr_queue.hpp"
#include "caf/intrusive/singly_linked.hpp"
#include "caf/intrusive/wdrr_fixed_multiplexed_queue.hpp"

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

class high_prio_queue;

struct inode_policy {
  using mapped_type = inode;

  using task_size_type = int;

  using deficit_type = int;

  using deleter_type = std::default_delete<mapped_type>;

  using unique_pointer = std::unique_ptr<mapped_type, deleter_type>;

  static inline task_size_type task_size(const mapped_type&) {
    return 1;
  }

  static inline size_t id_of(const inode& x) {
    return x.value % 3;
  }

  template <class Queue>
  deficit_type quantum(const Queue&, deficit_type x) {
    return x;
  }

  deficit_type quantum(const high_prio_queue&, deficit_type x) {
    return enable_priorities ? 2 * x : x;
  }

  bool enable_priorities = false;
};

class high_prio_queue : public drr_queue<inode_policy> {
public:
  using super = drr_queue<inode_policy>;

  using super::super;
};

using nested_queue_type = drr_queue<inode_policy>;

using queue_type = wdrr_fixed_multiplexed_queue<inode_policy,
                                                high_prio_queue,
                                                nested_queue_type,
                                                nested_queue_type>;

struct fetch_helper {
  std::string result;

  template <size_t I, class Queue>
  task_result operator()(std::integral_constant<size_t, I>, const Queue&,
                         inode& x) {
    if (!result.empty())
      result += ',';
    result += std::to_string(I);
    result += ':';
    result += to_string(x);
    return task_result::resume;
  }
};

struct fixture {
  inode_policy policy;
  queue_type queue{policy, policy, policy, policy};

  void fill(queue_type&) {
    // nop
  }

  template <class T, class... Ts>
  void fill(queue_type& q, T x, Ts... xs) {
    q.emplace_back(x);
    fill(q, xs...);
  }

  std::string fetch(int quantum) {
    std::string result;
    auto f = [&](size_t id, drr_queue<inode_policy>&, inode& x) {
      if (!result.empty())
        result += ',';
      result += std::to_string(id);
      result += ':';
      result += to_string(x);
      return task_result::resume;
    };
    queue.new_round(quantum, f);
    return result;
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(wdrr_fixed_multiplexed_queue_tests, fixture)

CAF_TEST(default_constructed) {
  CAF_REQUIRE_EQUAL(queue.empty(), true);
}

CAF_TEST(new_round) {
  fill(queue, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12);
  // Allow f to consume 2 items per nested queue.
  fetch_helper f;
  auto round_result = queue.new_round(2, f);
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(f.result, "0:3,0:6,1:1,1:4,2:2,2:5");
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  // Allow f to consume one more item from each queue.
  f.result.clear();
  round_result = queue.new_round(1, f);
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(f.result, "0:9,1:7,2:8");
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  // Allow f to consume the remainder, i.e., 12.
  f.result.clear();
  round_result = queue.new_round(1000, f);
  CAF_CHECK_EQUAL(round_result, make_new_round_result(true));
  CAF_CHECK_EQUAL(f.result, "0:12");
  CAF_REQUIRE_EQUAL(queue.empty(), true);
}

CAF_TEST(priorities) {
  queue.policy().enable_priorities = true;
  fill(queue, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  // Allow f to consume 2 items from the high priority and 1 item otherwise.
  CAF_CHECK_EQUAL(fetch(1), "0:3,0:6,1:1,2:2");
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  // Drain the high-priority queue with one item left per other queue.
  CAF_CHECK_EQUAL(fetch(1), "0:9,1:4,2:5");
  CAF_REQUIRE_EQUAL(queue.empty(), false);
  // Drain queue.
  CAF_CHECK_EQUAL(fetch(1000), "1:7,2:8");
  CAF_REQUIRE_EQUAL(queue.empty(), true);
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
  // Lists are iterated in order and 3 is stored in the first queue for
  // `x mod 3 == 0` values.
  CAF_CHECK_EQUAL(queue_to_string(), "3, 1, 2");
  queue.emplace_back(4);
  CAF_CHECK_EQUAL(queue_to_string(), "3, 1, 4, 2");
}

CAF_TEST(to_string) {
  CAF_CHECK_EQUAL(deep_to_string(queue), "[]");
  fill(queue, 1, 2, 3, 4);
  CAF_CHECK_EQUAL(deep_to_string(queue), "[3, 1, 4, 2]");
}

CAF_TEST_FIXTURE_SCOPE_END()
