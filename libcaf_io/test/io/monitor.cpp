/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE io.monitor

#include "caf/io/middleman.hpp"

#include "caf/test/io_dsl.hpp"

using namespace caf;

namespace {

struct fixture : point_to_point_fixture<> {
  actor observer;

  fixture() {
    mars_id = mars.sys.node();
  }

  // Connects earth and mars, storing the connection handles in earth_conn and
  // mars_conn.
  void connect() {
    auto acc = next_accept_handle();
    std::tie(earth_conn, mars_conn)
      = prepare_connection(earth, mars, "localhost", 8080, acc);
    CAF_CHECK_EQUAL(earth.publish(actor{earth.self}, 8080), 8080);
    CAF_CHECK(mars.remote_actor("localhost", 8080));
  }

  void disconnect() {
    anon_send(earth.bb, io::connection_closed_msg{earth_conn});
    earth.handle_io_event();
    anon_send(mars.bb, io::connection_closed_msg{mars_conn});
    mars.handle_io_event();
  }

  node_id mars_id;
  io::connection_handle earth_conn;
  io::connection_handle mars_conn;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(monitor_tests, fixture)

CAF_TEST(disconnects cause node_down_msg) {
  connect();
  earth.self->monitor(mars_id);
  run();
  disconnect();
  expect_on(earth, (node_down_msg),
            to(earth.self).with(node_down_msg{mars_id, error{}}));
  CAF_CHECK(earth.self->mailbox().empty());
}

CAF_TEST(node_down_msg calls the special node_down_handler) {
  connect();
  bool node_down_handler_called = false;
  auto observer = earth.sys.spawn([&](event_based_actor* self) -> behavior {
    self->monitor(mars_id);
    self->set_node_down_handler([&](node_down_msg& dm) {
      CAF_CHECK_EQUAL(dm.node, mars_id);
      node_down_handler_called = true;
    });
    return [] {};
  });
  run();
  disconnect();
  expect_on(earth, (node_down_msg),
            to(observer).with(node_down_msg{mars_id, error{}}));
  CAF_CHECK(node_down_handler_called);
}

CAF_TEST(calling monitor n times produces n node_down_msg) {
  connect();
  for (int i = 0; i < 5; ++i)
    earth.self->monitor(mars_id);
  run();
  disconnect();
  for (int i = 0; i < 5; ++i)
    expect_on(earth, (node_down_msg),
              to(earth.self).with(node_down_msg{mars_id, error{}}));
  CAF_CHECK_EQUAL(earth.self->mailbox().size(), 0u);
}

CAF_TEST(each demonitor only cancels one node_down_msg) {
  connect();
  for (int i = 0; i < 5; ++i)
    earth.self->monitor(mars_id);
  run();
  earth.self->demonitor(mars_id);
  run();
  disconnect();
  for (int i = 0; i < 4; ++i)
    expect_on(earth, (node_down_msg),
              to(earth.self).with(node_down_msg{mars_id, error{}}));
  CAF_CHECK_EQUAL(earth.self->mailbox().size(), 0u);
}

CAF_TEST_FIXTURE_SCOPE_END()
