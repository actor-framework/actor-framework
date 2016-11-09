/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#define CAF_SUITE io_datagram
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"
#include "caf/detail/build_config.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

constexpr auto host = "127.0.0.1";
constexpr uint16_t port = 1234;
constexpr auto uri_no_port = "udp://127.0.0.1";
// constexpr auto uri_with_port = "udp://127.0.0.1:1234";

class config : public actor_system_config {
public:
  config() {
    load<io::middleman>();
    add_message_type<vector<int>>("vector<int>");
    actor_system_config::parse(test::engine::argc(),
                               test::engine::argv());
  }
};

struct fixture {
  //fixture() {
  //  CAF_SET_LOGGER_SYS(&server_side);
  //    CAF_SET_LOGGER_SYS(&client_side);
  //}
  config server_side_config;
  actor_system server_side{server_side_config};
  config client_side_config;
  actor_system client_side{client_side_config};
  io::middleman& server_side_mm = server_side.middleman();
  io::middleman& client_side_mm = client_side.middleman();
};

behavior make_pong_behavior() {
  return {
    [](int val) -> int {
      CAF_MESSAGE("pong with " << ++val);
      return val;
    }
  };
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(datagrams, fixture)

CAF_TEST(test_datagram_sinks) {
  auto& mp = client_side_mm.backend();
  auto hdl = client_side_mm.named_broker<basp_broker>(atom("BASP"));
  auto basp = static_cast<basp_broker*>(actor_cast<abstract_actor*>(hdl));
  CAF_MESSAGE("Calling new_datagram_sink");
  auto res1 = mp.new_datagram_sink(host, port + 0);
  CAF_REQUIRE(res1);
  CAF_MESSAGE("Calling assign_datagram_sink");
  auto res2 = mp.assign_datagram_sink(basp, *res1);
  CAF_REQUIRE(res2);
  CAF_MESSAGE("Calling add_datagram_sink");
  auto res3 = mp.add_datagram_sink(basp, host, port + 1);
  CAF_REQUIRE(res3);
}

CAF_TEST(test_datagram_sources) {
  auto& mp = client_side_mm.backend();
  auto hdl = client_side_mm.named_broker<basp_broker>(atom("BASP"));
  auto basp = static_cast<basp_broker*>(actor_cast<abstract_actor*>(hdl));
  CAF_MESSAGE("Calling new_datagram_source");
  auto res1 = mp.new_datagram_source(port + 0);
  CAF_REQUIRE(res1);
  CAF_MESSAGE("Calling assign_datagram_source");
  auto res2 = mp.assign_datagram_source(basp, res1->first);
  CAF_REQUIRE(res2);
  CAF_MESSAGE("Calling add_datagram_source");
  auto res3 = mp.add_datagram_source(basp, port + 1, nullptr);
  CAF_REQUIRE(res3);
}

CAF_TEST(test_datagram_publish) {
  auto pong = client_side.spawn(make_pong_behavior);
  auto res1 = io::uri::make(uri_no_port);
  CAF_REQUIRE(res1);
  auto res2 = client_side_mm.publish(pong, *res1);
  CAF_REQUIRE(res2);
  CAF_MESSAGE("Published pong on port: " + to_string(*res2));
  anon_send_exit(pong, exit_reason::user_shutdown);
}

CAF_TEST(test_datagram_remote_actor) {
  auto pong = server_side.spawn(make_pong_behavior);
  auto res1 = io::uri::make(uri_no_port);
  CAF_REQUIRE(res1);
  auto res2 = server_side_mm.publish(pong, *res1);
  CAF_CHECK(res2);
  auto res3 = io::uri::make(string(uri_no_port) + ":" + to_string(*res2));
  CAF_CHECK(res3);
  CAF_MESSAGE("Published pong on: " + to_string(*res3) + ".");

  CAF_SET_LOGGER_SYS(&server_side);
  CAF_MESSAGE("Local call to remote actor should acquire the actor.");
  auto res4 = server_side_mm.remote_actor(*res3);
  CAF_CHECK(res4);

  CAF_MESSAGE("Checking from different actor system next.");
  auto res5 = client_side_mm.remote_actor(*res3);
  CAF_CHECK(res5);

  anon_send_exit(pong, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
