// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/const_typed_message_view.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/message.hpp"

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::deterministic) {

TEST("const message views never detach their content") {
  auto msg1 = make_message(1, 2, 3, "four");
  auto msg2 = msg1;
  require(msg1.cptr() == msg2.cptr());
  require((msg1.match_elements<int, int, int, std::string>()));
  const_typed_message_view<int, int, int, std::string> view{msg1};
  require(msg1.cptr() == msg2.cptr());
}

TEST("const message views allow access via get") {
  auto msg = make_message(1, 2, 3, "four");
  require((msg.match_elements<int, int, int, std::string>()));
  const_typed_message_view<int, int, int, std::string> view{msg};
  check_eq(get<0>(view), 1);
  check_eq(get<1>(view), 2);
  check_eq(get<2>(view), 3);
  check_eq(get<3>(view), "four");
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
