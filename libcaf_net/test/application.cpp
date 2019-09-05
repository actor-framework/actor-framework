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

#define CAF_SUITE basp.application

#include "caf/net/basp/application.hpp"

#include "caf/test/dsl.hpp"

#include <vector>

#include "caf/byte.hpp"
#include "caf/net/basp/connection_state.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/none.hpp"

using namespace caf;
using namespace caf::net;

#define REQUIRE_OK(statement)                                                  \
  if (auto err = statement)                                                    \
    CAF_FAIL("failed to serialize data: " << sys.render(err));

namespace {

struct fixture : test_coordinator_fixture<> {
  using buffer_type = std::vector<byte>;

  fixture() {
    REQUIRE_OK(app.init(*this));
  }

  template <class... Ts>
  void set_input(const Ts&...xs) {
    serializer_impl<buffer_type> sink{nullptr, input};
    REQUIRE_OK(sink(xs...));
  }

  buffer_type input;

  buffer_type output;

  basp::application app;
};

} // namespace


CAF_TEST_FIXTURE_SCOPE(application_tests, fixture)

CAF_TEST(invalid magic number) {
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_magic_number);
  set_input(basp::magic_number + 1);
  CAF_CHECK_EQUAL(app.handle_data(*this, input),
                  basp::ec::invalid_magic_number);
}

CAF_TEST(handshake sequence) {
  basp::application app;
  REQUIRE_OK(app.init(*this));
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_magic_number);
  set_input(basp::magic_number);
  REQUIRE_OK(app.handle_data(*this, input));
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
}

CAF_TEST_FIXTURE_SCOPE_END()
