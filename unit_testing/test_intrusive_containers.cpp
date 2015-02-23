/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include <iterator>

#include "test.hpp"

#include "caf/policy/prioritizing.hpp"
#include "caf/detail/single_reader_queue.hpp"

using std::begin;
using std::end;

using namespace caf;

namespace {

size_t s_iint_instances = 0;

} // namespace <anonymous>

struct iint {
  iint* next;
  iint* prev;
  int value;
  bool is_high_priority() const {
    return value % 2 == 0;
  }
  iint(int val = 0) : next(nullptr), prev(nullptr), value(val) {
    ++s_iint_instances;
  }
  ~iint() {
    --s_iint_instances;
  }
};

inline bool operator==(const iint& lhs, const iint& rhs) {
  return lhs.value == rhs.value;
}

inline bool operator==(const iint& lhs, int rhs) { return lhs.value == rhs; }

inline bool operator==(int lhs, const iint& rhs) { return lhs == rhs.value; }

using iint_queue = detail::single_reader_queue<iint>;

struct pseudo_actor {
  using mailbox_type = detail::single_reader_queue<iint>;
  using uptr = mailbox_type::unique_pointer;
  mailbox_type mbox;

  mailbox_type& mailbox() {
    return mbox;
  }

  static policy::invoke_message_result invoke_message(uptr& ptr, int i) {
    if (ptr->value == 1) {
      ptr.reset();
      return policy::im_dropped;
    }
    if (ptr->value == i) {
      // call reset on some of our messages
      if (ptr->is_high_priority()) {
        ptr.reset();
      }
      return policy::im_success;
    }
    return policy::im_skipped;
  }

  template <class Policy>
  policy::invoke_message_result invoke_message(uptr& ptr, Policy& policy, int i,
                                               std::vector<int>& remaining) {
    auto res = invoke_message(ptr, i);
    if (res == policy::im_success && !remaining.empty()) {
      auto next = remaining.front();
      remaining.erase(remaining.begin());
      policy.invoke_from_cache(this, policy, next, remaining);
    }
    return res;
  }
};

void test_prioritizing_dequeue() {
  pseudo_actor self;
  auto baseline = s_iint_instances;
  CAF_CHECK_EQUAL(baseline, 3); // "begin", "separator", and "end"
  self.mbox.enqueue(new iint(1));
  self.mbox.enqueue(new iint(2));
  self.mbox.enqueue(new iint(3));
  self.mbox.enqueue(new iint(4));
  CAF_CHECK_EQUAL(baseline + 4, s_iint_instances);
  policy::prioritizing policy;
  // first "high priority message"
  CAF_CHECK_EQUAL(self.mbox.count(), 4);
  CAF_CHECK_EQUAL(policy.has_next_message(&self), true);
  auto x = policy.next_message(&self);
  CAF_CHECK_EQUAL(x->value, 2);
  // second "high priority message"
  CAF_CHECK_EQUAL(self.mbox.count(), 3);
  CAF_CHECK_EQUAL(policy.has_next_message(&self), true);
  x = policy.next_message(&self);
  CAF_CHECK_EQUAL(x->value, 4);
  // first "low priority message"
  CAF_CHECK_EQUAL(self.mbox.count(), 2);
  CAF_CHECK_EQUAL(policy.has_next_message(&self), true);
  x = policy.next_message(&self);
  CAF_CHECK_EQUAL(x->value, 1);
  // first "low priority message"
  CAF_CHECK_EQUAL(self.mbox.count(), 1);
  CAF_CHECK_EQUAL(policy.has_next_message(&self), true);
  x = policy.next_message(&self);
  CAF_CHECK_EQUAL(x->value, 3);
  x.reset();
  // back to baseline
  CAF_CHECK_EQUAL(self.mbox.count(), 0);
  CAF_CHECK_EQUAL(baseline, s_iint_instances);
}

template <class Policy>
void test_invoke_from_cache() {
  pseudo_actor self;
  auto baseline = s_iint_instances;
  CAF_CHECK_EQUAL(baseline, 3); // "begin", "separator", and "end"
  self.mbox.enqueue(new iint(1));
  self.mbox.enqueue(new iint(2));
  self.mbox.enqueue(new iint(3));
  self.mbox.enqueue(new iint(4));
  self.mbox.enqueue(new iint(5));
  Policy policy;
  CAF_CHECK_EQUAL(self.mbox.count(), 5);
  // fill cache
  auto ptr = policy.next_message(&self);
  while (ptr) {
    policy.push_to_cache(&self, std::move(ptr));
    ptr = policy.next_message(&self);
  }
  CAF_CHECK_EQUAL(self.mbox.count(), 5);
  // dequeue 3, 2, 4, 1, note: 1 is dropped on first dequeue
  int expected;
  //std::vector<int> expected{3, 2, 4, 5};
  size_t remaining = 4;
  //for (auto& i : expected) {
    CAF_CHECK(policy.invoke_from_cache(&self, expected = 3));
    CAF_CHECK_EQUAL(self.mbox.count(), --remaining);
    CAF_CHECK(policy.invoke_from_cache(&self, expected = 2));
    CAF_CHECK_EQUAL(self.mbox.count(), --remaining);
    CAF_CHECK(policy.invoke_from_cache(&self, expected = 4));
    CAF_CHECK_EQUAL(self.mbox.count(), --remaining);
    CAF_CHECK(policy.invoke_from_cache(&self, expected = 5));
    CAF_CHECK_EQUAL(self.mbox.count(), --remaining);
  //}
}

template <class Policy>
void test_recursive_invoke_from_cache() {
  pseudo_actor self;
  auto baseline = s_iint_instances;
  CAF_CHECK_EQUAL(baseline, 3); // "begin", "separator", and "end"
  self.mbox.enqueue(new iint(1));
  self.mbox.enqueue(new iint(2));
  self.mbox.enqueue(new iint(3));
  self.mbox.enqueue(new iint(4));
  self.mbox.enqueue(new iint(5));
  Policy policy;
  CAF_CHECK_EQUAL(self.mbox.count(), 5);
  // fill cache
  auto ptr = policy.next_message(&self);
  while (ptr) {
    policy.push_to_cache(&self, std::move(ptr));
    ptr = policy.next_message(&self);
  }
  CAF_CHECK_EQUAL(self.mbox.count(), 5);
  // dequeue 3, 2, 4, 1, note: 1 is dropped on first dequeue
  std::vector<int> remaining{2, 4, 5};
  int first = 3;
  policy.invoke_from_cache(&self, policy, first, remaining);
  CAF_CHECK_EQUAL(self.mbox.count(), 0);
}

int main() {
  CAF_TEST(test_intrusive_containers);
  CAF_CHECKPOINT();
  test_prioritizing_dequeue();
  CAF_PRINT("test_invoke_from_cache<policy::prioritizing>");
  test_invoke_from_cache<policy::prioritizing>();
  CAF_PRINT("test_invoke_from_cache<policy::not_prioritizing>");
  test_invoke_from_cache<policy::not_prioritizing>();
  CAF_PRINT("test_recursive_invoke_from_cache<policy::prioritizing>");
  test_recursive_invoke_from_cache<policy::prioritizing>();
  CAF_PRINT("test_recursive_invoke_from_cache<policy::not_prioritizing>");
  test_recursive_invoke_from_cache<policy::not_prioritizing>();
  return CAF_TEST_RESULT();
}
