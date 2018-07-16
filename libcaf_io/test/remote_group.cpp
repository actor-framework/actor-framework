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

#define CAF_SUITE io_dynamic_remote_group
#include "caf/test/io_dsl.hpp"

#include <vector>
#include <algorithm>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

class config : public caf::actor_system_config {
public:
  config() {
    load<caf::io::middleman>();
    add_message_type<std::vector<actor>>("std::vector<actor>");
  }
};

const uint16_t port = 8080;

const char* server = "mars";

const char* group_name = "foobar";

size_t received_messages = 0u;

behavior group_receiver(event_based_actor* self) {
  self->set_default_handler(reflect_and_quit);
  return {
    [](ok_atom) {
      ++received_messages;
    }
  };
}

// Our server is `mars` and our client is `earth`.
struct fixture : point_to_point_fixture<test_coordinator_fixture<config>> {
  fixture() {
    prepare_connection(mars, earth, server, port);
  }

  ~fixture() {
    for (auto& receiver : receivers)
      anon_send_exit(receiver, exit_reason::user_shutdown);
  }

  void spawn_receivers(planet_type& planet, group grp, size_t count) {
    for (size_t i = 0; i < count; ++i)
      receivers.emplace_back(planet.sys.spawn_in_group(grp, group_receiver));
  }

  std::vector<actor> receivers;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(dynamic_remote_group_tests, fixture)

CAF_TEST(publish_local_groups) {
  loop_after_next_enqueue(mars);
  CAF_CHECK_EQUAL(mars.sys.middleman().publish_local_groups(port), port);
}

CAF_TEST(connecting to remote group) {
  CAF_MESSAGE("publish local groups on mars");
  loop_after_next_enqueue(mars);
  CAF_CHECK_EQUAL(mars.sys.middleman().publish_local_groups(port), port);
  CAF_MESSAGE("call remote_group on earth");
  loop_after_next_enqueue(earth);
  auto grp = unbox(earth.mm.remote_group(group_name, server, port));
  CAF_CHECK(grp);
  CAF_CHECK_EQUAL(grp->get()->identifier(), group_name);
}

CAF_TEST_DISABLED(message transmission) {
  CAF_MESSAGE("spawn 5 receivers on mars");
  auto mars_grp = mars.sys.groups().get_local(group_name);
  spawn_receivers(mars, mars_grp, 5u);
  CAF_MESSAGE("publish local groups on mars");
  loop_after_next_enqueue(mars);
  CAF_CHECK_EQUAL(mars.sys.middleman().publish_local_groups(port), port);
  CAF_MESSAGE("call remote_group on earth");
  loop_after_next_enqueue(earth);
  auto earth_grp = unbox(earth.mm.remote_group(group_name, server, port));
  CAF_MESSAGE("spawn 5 more receivers on earth");
  spawn_receivers(earth, earth_grp, 5u);
  CAF_MESSAGE("send message on mars and expect 10 handled messages total");
  {
    received_messages = 0u;
    scoped_actor self{mars.sys};
    self->send(mars_grp, ok_atom::value);
    exec_all();
    CAF_CHECK_EQUAL(received_messages, 10u);
  }
  CAF_MESSAGE("send message on earth and again expect 10 handled messages");
  {
    received_messages = 0u;
    scoped_actor self{earth.sys};
    self->send(earth_grp, ok_atom::value);
    exec_all();
    CAF_CHECK_EQUAL(received_messages, 10u);
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
