/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#define CAF_SUITE type_erased_tuple
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"
#include "caf/make_type_erased_tuple_view.hpp"

using namespace std;
using namespace caf;

CAF_TEST(get_as_tuple) {
  int x = 1;
  int y = 2;
  int z = 3;
  auto tup = make_type_erased_tuple_view(x, y, z);
  auto xs = tup.get_as_tuple<int, int, int>();
  CAF_CHECK_EQUAL(xs, std::make_tuple(1, 2, 3));
}

