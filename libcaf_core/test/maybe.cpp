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

#define CAF_SUITE maybe
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/maybe.hpp"

using namespace std;
using namespace caf;

namespace {

struct qwertz {
  qwertz(int i, int j) : i_(i), j_(j) {
    // nop
  }
  int i_;
  int j_;
};

bool operator==(const qwertz& lhs, const qwertz& rhs) {
  return lhs.i_ == rhs.i_ && lhs.j_ == rhs.j_;
}

} // namespace <anonymous>

CAF_TEST(empties) {
  maybe<int> i;
  maybe<int> j;
  CAF_CHECK(i == j);
  CAF_CHECK(!(i != j));
}

CAF_TEST(unequal) {
  maybe<int> i = 5;
  maybe<int> j = 6;
  CAF_CHECK(!(i == j));
  CAF_CHECK(i != j);
}

CAF_TEST(distinct_types) {
  maybe<int> i;
  maybe<double> j;
  CAF_CHECK(i == j);
  CAF_CHECK(!(i != j));
}

CAF_TEST(custom_type_none) {
  maybe<qwertz> i;
  CAF_CHECK(i == none);
}

CAF_TEST(custom_type_engaged) {
  qwertz obj{1, 2};
  maybe<qwertz> j = obj;
  CAF_CHECK(j != none);
  CAF_CHECK(obj == j);
  CAF_CHECK(j == obj );
  CAF_CHECK(obj == *j);
  CAF_CHECK(*j == obj);
}

CAF_TEST(error_construct_and_assign) {
  auto f = []() -> maybe<int> {
    return std::errc::invalid_argument;
  };
  auto val = f();
  CAF_CHECK(! val && val.error() == std::errc::invalid_argument);
  val = 42;
  CAF_CHECK(val && *val == 42);
  val = std::errc::state_not_recoverable;
  CAF_CHECK(! val && val.error() == std::errc::state_not_recoverable);
}

CAF_TEST(maybe_void) {
  maybe<void> m;
  CAF_CHECK(! m);
  CAF_CHECK(m.empty());
  CAF_CHECK(! m.error());
  // Assign erroneous state.
  m = std::errc::invalid_argument;
  CAF_CHECK(! m);
  CAF_CHECK(! m.empty());
  CAF_CHECK(m.error());
  CAF_CHECK(m.error() == std::errc::invalid_argument);
  // Implicit construction.
  auto f = []() -> maybe<void> {
    return std::errc::invalid_argument;
  };
  auto val = f();
  CAF_CHECK(! val && val.error() == std::errc::invalid_argument);
}
