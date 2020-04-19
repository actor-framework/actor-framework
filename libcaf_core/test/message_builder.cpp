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

#define CAF_SUITE message_builder

#include "caf/message_builder.hpp"

#include "core-test.hpp"

#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "caf/message.hpp"
#include "caf/type_id_list.hpp"

using namespace caf;

#define STEP(message)                                                          \
  CAF_MESSAGE(message);                                                        \
  if (true)

CAF_TEST(message builder can build messages incrermenetally) {
  message_builder builder;
  CAF_CHECK(builder.empty());
  CAF_CHECK(builder.to_message().empty());
  CAF_CHECK_EQUAL(builder.size(), 0u);
  STEP("after adding 1, the message is (1)") {
    builder.append(int32_t{1});
    auto msg = builder.to_message();
    CAF_CHECK_EQUAL(builder.size(), 1u);
    CAF_CHECK_EQUAL(msg.types(), make_type_id_list<int32_t>());
    CAF_CHECK_EQUAL(to_string(msg.types()), "[int32_t]");
    CAF_CHECK_EQUAL(to_string(msg), "(1)");
  }
  STEP("after adding [2, 3], the message is (1, 2, 3)") {
    std::vector<int32_t> xs{2, 3};
    builder.append(xs.begin(), xs.end());
    CAF_CHECK_EQUAL(builder.size(), 3u);
    auto msg = builder.to_message();
    CAF_CHECK_EQUAL(msg.types(),
                    (make_type_id_list<int32_t, int32_t, int32_t>()));
    CAF_CHECK_EQUAL(to_string(msg.types()), "[int32_t, int32_t, int32_t]");
    CAF_CHECK_EQUAL(to_string(msg), "(1, 2, 3)");
  }
  STEP("moving the content to a message produces the same message again") {
    auto msg = builder.move_to_message();
    CAF_CHECK_EQUAL(msg.types(),
                    (make_type_id_list<int32_t, int32_t, int32_t>()));
    CAF_CHECK_EQUAL(to_string(msg.types()), "[int32_t, int32_t, int32_t]");
    CAF_CHECK_EQUAL(to_string(msg), "(1, 2, 3)");
  }
}
