// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
  MESSAGE(message);                                                            \
  if (true)

CAF_TEST(message builder can build messages incrermenetally) {
  message_builder builder;
  CHECK(builder.empty());
  CHECK(builder.to_message().empty());
  CHECK_EQ(builder.size(), 0u);
  STEP("after adding 1, the message is (1)") {
    builder.append(int32_t{1});
    auto msg = builder.to_message();
    CHECK_EQ(builder.size(), 1u);
    CHECK_EQ(msg.types(), make_type_id_list<int32_t>());
    CHECK_EQ(to_string(msg.types()), "[int32_t]");
    CHECK_EQ(to_string(msg), "message(1)");
  }
  STEP("after adding [2, 3], the message is (1, 2, 3)") {
    std::vector<int32_t> xs{2, 3};
    builder.append(xs.begin(), xs.end());
    CHECK_EQ(builder.size(), 3u);
    auto msg = builder.to_message();
    CHECK_EQ(msg.types(), (make_type_id_list<int32_t, int32_t, int32_t>()));
    CHECK_EQ(to_string(msg.types()), "[int32_t, int32_t, int32_t]");
    CHECK_EQ(to_string(msg), "message(1, 2, 3)");
  }
  STEP("moving the content to a message produces the same message again") {
    auto msg = builder.move_to_message();
    CHECK_EQ(msg.types(), (make_type_id_list<int32_t, int32_t, int32_t>()));
    CHECK_EQ(to_string(msg.types()), "[int32_t, int32_t, int32_t]");
    CHECK_EQ(to_string(msg), "message(1, 2, 3)");
  }
}
