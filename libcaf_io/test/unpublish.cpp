/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#define CAF_SUITE io_unpublish
#include "caf/test/unit_test.hpp"

#include <new>
#include <thread>
#include <atomic>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

std::atomic<long> s_dtor_called;

class dummy : public event_based_actor {
public:
  dummy(actor_config& cfg) : event_based_actor(cfg) {
    // nop
  }

  ~dummy() {
    ++s_dtor_called;
  }

  behavior make_behavior() override {
    return {
      others >> [&] {
        CAF_ERROR("Unexpected message");
      }
    };
  }
};

struct fixture {
  fixture() {
    new (&system) actor_system(actor_system_config{test::engine::argc(),
                                                   test::engine::argv()}
                               .load<io::middleman>());
    testee = system.spawn<dummy>();
  }

  ~fixture() {
    anon_send_exit(testee, exit_reason::user_shutdown);
    testee = invalid_actor;
    system.~actor_system();
    CAF_CHECK_EQUAL(s_dtor_called.load(), 2);
  }

  maybe<actor> remote_actor(const char* hostname, uint16_t port) {
    maybe<actor> result;
    scoped_actor self{system, true};
    self->request(system.middleman().actor_handle(), infinite,
                  connect_atom::value, hostname, port).receive(
      [&](ok_atom, node_id&, actor_addr& res, std::set<std::string>& xs) {
        CAF_REQUIRE(xs.empty());
        result = actor_cast<actor>(std::move(res));
      },
      [&](error& err) {
        result = std::move(err);
      }
    );
    return result;
  }

  union { actor_system system; }; // manually control ctor/dtor
  actor testee;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(unpublish_tests, fixture)

CAF_TEST(unpublishing) {
  auto port = system.middleman().publish(testee, 0);
  CAF_REQUIRE(port);
  CAF_MESSAGE("published actor on port " << *port);
  CAF_MESSAGE("test invalid unpublish");
  auto testee2 = system.spawn<dummy>();
  system.middleman().unpublish(testee2, *port);
  auto x0 = remote_actor("127.0.0.1", *port);
  CAF_CHECK(x0 != testee2);
  CAF_CHECK(x0 == testee);
  anon_send_exit(testee2, exit_reason::kill);
  CAF_MESSAGE("unpublish testee");
  system.middleman().unpublish(testee, *port);
  CAF_MESSAGE("check whether testee is still available via cache");
  auto x1 = remote_actor("127.0.0.1", *port);
  CAF_CHECK(x1 && *x1 == testee);
  CAF_MESSAGE("fake dead of testee and check if testee becomes unavailable");
  anon_send(system.middleman().actor_handle(), down_msg{testee.address(),
                                                        exit_reason::normal});
  // must fail now
  auto x2 = remote_actor("127.0.0.1", *port);
  CAF_CHECK(! x2);
}

CAF_TEST_FIXTURE_SCOPE_END()
