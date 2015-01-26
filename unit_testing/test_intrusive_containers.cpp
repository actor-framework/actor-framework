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

#include "caf/detail/single_reader_queue.hpp"

using std::begin;
using std::end;

namespace {
size_t s_iint_instances = 0;
}

struct iint {
  iint* next;
  int value;
  inline iint(int val = 0) : next(nullptr), value(val) { ++s_iint_instances; }
  ~iint() { --s_iint_instances; }

};

inline bool operator==(const iint& lhs, const iint& rhs) {
  return lhs.value == rhs.value;
}

inline bool operator==(const iint& lhs, int rhs) { return lhs.value == rhs; }

inline bool operator==(int lhs, const iint& rhs) { return lhs == rhs.value; }

using iint_queue = caf::detail::single_reader_queue<iint>;

int main() {
  CAF_TEST(test_intrusive_containers);

  caf::detail::single_reader_queue<iint> q;
  q.enqueue(new iint(1));
  q.enqueue(new iint(2));
  q.enqueue(new iint(3));

  CAF_CHECK_EQUAL(3, s_iint_instances);

  auto x = q.try_pop();
  CAF_CHECK_EQUAL(x->value, 1);
  delete x;
  x = q.try_pop();
  CAF_CHECK_EQUAL(x->value, 2);
  delete x;
  x = q.try_pop();
  CAF_CHECK_EQUAL(x->value, 3);
  delete x;
  x = q.try_pop();
  CAF_CHECK(x == nullptr);

  return CAF_TEST_RESULT();
}
