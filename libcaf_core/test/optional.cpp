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

#define CAF_SUITE optional
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/optional.hpp"

using namespace std;
using namespace caf;

CAF_TEST(test_optional) {
  {
    optional<int> i,j;
    CAF_CHECK(i == j);
    CAF_CHECK(!(i != j));
  }
  {
    optional<int> i = 5;
    optional<int> j = 6;
    CAF_CHECK(!(i == j));
    CAF_CHECK(i != j);
  }
  {
    optional<int> i;
    optional<double> j;
    CAF_CHECK(i == j);
    CAF_CHECK(!(i != j));
  }
  {
    struct qwertz {
      qwertz(int i, int j) : m_i(i), m_j(j) {
        CAF_MESSAGE("called ctor of `qwertz`");
      }
      int m_i;
      int m_j;
    };
    {
      optional<qwertz> i;
      CAF_CHECK(i.empty());
    }
    {
      qwertz obj (1,2);
      optional<qwertz> j(obj);
      CAF_CHECK(!j.empty());
    }
    {
      optional<qwertz> i = qwertz(1,2);
      CAF_CHECK(!i.empty());
      optional<qwertz> j = { { 1, 2 } };
      CAF_CHECK(!j.empty());
    }
  }
}
