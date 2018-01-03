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

#define CAF_SUITE to_string
#include "caf/test/unit_test.hpp"

#include "caf/to_string.hpp"

using namespace std;
using namespace caf;

CAF_TEST(buffer) {
  // Use `signed char` explicitly to make sure all compilers agree.
  std::vector<signed char> buf;
  CAF_CHECK_EQUAL(deep_to_string(buf), "[]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "00");
  buf.push_back(-1);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF");
  buf.push_back(0);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1, 0]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF00");
  buf.push_back(127);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1, 0, 127]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF007F");
  buf.push_back(10);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1, 0, 127, 10]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF007F0A");
  buf.push_back(16);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1, 0, 127, 10, 16]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF007F0A10");
}
