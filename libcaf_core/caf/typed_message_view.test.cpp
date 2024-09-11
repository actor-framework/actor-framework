// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/typed_message_view.hpp"

#include "caf/test/test.hpp"

#include "caf/message.hpp"

using namespace caf;

TEST("message views detach their content") {
  auto msg1 = make_message(1, 2, 3, "four");
  auto msg2 = msg1;
  require_eq(msg1.cptr(), msg2.cptr());
  require((msg1.match_elements<int, int, int, std::string>()));
  typed_message_view<int, int, int, std::string> view{msg1};
  require_ne(msg1.cptr(), msg2.cptr());
}

TEST("message views allow access via get") {
  auto msg = make_message(1, 2, 3, "four");
  require((msg.match_elements<int, int, int, std::string>()));
  typed_message_view<int, int, int, std::string> view{msg};
  check_eq(get<0>(view), 1);
  check_eq(get<1>(view), 2);
  check_eq(get<2>(view), 3);
  check_eq(get<3>(view), "four");
}

TEST("message views allow mutating elements") {
  auto msg1 = make_message(1, 2, 3, "four");
  auto msg2 = msg1;
  require((msg1.match_elements<int, int, int, std::string>()));
  typed_message_view<int, int, int, std::string> view{msg1};
  get<0>(view) = 10;
  check_eq(msg1.get_as<int>(0), 10);
  check_eq(msg2.get_as<int>(0), 1);
}
