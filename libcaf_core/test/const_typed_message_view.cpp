/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE const_typed_message_view

#include "caf/const_typed_message_view.hpp"

#include "caf/test/dsl.hpp"

#include "caf/make_message.hpp"
#include "caf/message.hpp"

using namespace caf;

CAF_TEST(const message views never detach their content) {
  auto msg1 = make_message(1, 2, 3, "four");
  auto msg2 = msg1;
  CAF_REQUIRE(msg1.cvals().get() == msg2.cvals().get());
  CAF_REQUIRE(msg1.match_elements<int, int, int, std::string>());
  const_typed_message_view<int, int, int, std::string> view{msg1};
  CAF_REQUIRE(msg1.cvals().get() == msg2.cvals().get());
}

CAF_TEST(const message views allow access via get) {
  auto msg = make_message(1, 2, 3, "four");
  CAF_REQUIRE(msg.match_elements<int, int, int, std::string>());
  const_typed_message_view<int, int, int, std::string> view{msg};
  CAF_CHECK_EQUAL(get<0>(view), 1);
  CAF_CHECK_EQUAL(get<1>(view), 2);
  CAF_CHECK_EQUAL(get<2>(view), 3);
  CAF_CHECK_EQUAL(get<3>(view), "four");
}
