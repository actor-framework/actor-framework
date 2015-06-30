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

#define CAF_SUITE variant
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/variant.hpp"

using namespace std;
using namespace caf;

struct tostring_visitor : static_visitor<string> {
  template <class T>
  inline string operator()(const T& value) {
    return to_string(value);
  }
};

CAF_TEST(string_convertible) {
  tostring_visitor tv;
  // test never-empty guarantee, i.e., expect default-constucted first arg
  variant<int, float> v1;
  CAF_CHECK_EQUAL(apply_visitor(tv, v1), "0");
  variant<int, float> v2 = 42;
  CAF_CHECK_EQUAL(apply_visitor(tv, v2), "42");
  v2 = 0.2f;
  CAF_CHECK_EQUAL(apply_visitor(tv, v2), to_string(0.2f));
}
