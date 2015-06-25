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

#include "caf/config.hpp"

#define CAF_SUITE optional
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/optional.hpp"

using namespace std;
using namespace caf;

CAF_TEST(empties) {
  optional<int> i;
  optional<int> j;
  CAF_CHECK(i == j);
  CAF_CHECK(!(i != j));
}

CAF_TEST(unequal) {
  optional<int> i = 5;
  optional<int> j = 6;
  CAF_CHECK(!(i == j));
  CAF_CHECK(i != j);
}

CAF_TEST(distinct_types) {
  optional<int> i;
  optional<double> j;
  CAF_CHECK(i == j);
  CAF_CHECK(!(i != j));
}

struct qwertz {
  qwertz(int i, int j) : i_(i), j_(j) {
    // nop
  }
  int i_;
  int j_;
};

inline bool operator==(const qwertz& lhs, const qwertz& rhs) {
  return lhs.i_ == rhs.i_ && lhs.j_ == rhs.j_;
}

CAF_TEST(custom_type_none) {
  optional<qwertz> i;
  CAF_CHECK(i == none);
}

CAF_TEST(custom_type_engaged) {
  qwertz obj{1, 2};
  optional<qwertz> j = obj;
  CAF_CHECK(j != none);
  CAF_CHECK(obj == j);
  CAF_CHECK(j == obj );
  CAF_CHECK(obj == *j);
  CAF_CHECK(*j == obj);
}

CAF_TEST(test_optional) {
  optional<qwertz> i = qwertz(1,2);
  CAF_CHECK(! i.empty());
  optional<qwertz> j = { { 1, 2 } };
  CAF_CHECK(! j.empty());
}
