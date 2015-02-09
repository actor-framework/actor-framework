/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

namespace {
size_t s_iint_instances = 0;
}

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

using iint_queue = caf::detail::single_reader_queue<iint>;

struct pseudo_actor {
  using mailbox_type = caf::detail::single_reader_queue<iint>;
  mailbox_type mbox;
  mailbox_type& mailbox() {
    return mbox;
  }
};

int main() {
  CAF_TEST(test_intrusive_containers);
  pseudo_actor self;
  auto baseline = s_iint_instances;
  CAF_CHECK_EQUAL(baseline, 3); // "begin", "separator", and "end"
  self.mbox.enqueue(new iint(1));
  self.mbox.enqueue(new iint(2));
  self.mbox.enqueue(new iint(3));
  self.mbox.enqueue(new iint(4));
  CAF_CHECK_EQUAL(baseline + 4, s_iint_instances);
  caf::policy::prioritizing policy;
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
  return CAF_TEST_RESULT();
}
