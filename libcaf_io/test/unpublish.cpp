/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

  ~dummy() override {
    ++s_dtor_called;
  }

  behavior make_behavior() override {
    return {
      [] {
        // nop
      }
    };
  }
};

struct fixture {
  fixture() {
    new (&system) actor_system(cfg.load<io::middleman>()
                                  .parse(test::engine::argc(),
                                         test::engine::argv()));
    testee = system.spawn<dummy>();
  }

  ~fixture() {
    anon_send_exit(testee, exit_reason::user_shutdown);
    destroy(testee);
    system.~actor_system();
    CAF_CHECK_EQUAL(s_dtor_called.load(), 2);
  }

  actor remote_actor(const char* hostname, uint16_t port,
                     bool expect_fail = false) {
    actor result;
    scoped_actor self{system, true};
    self->request(system.middleman().actor_handle(), infinite,
                  connect_atom::value, hostname, port).receive(
      [&](node_id&, strong_actor_ptr& res, std::set<std::string>& xs) {
        CAF_REQUIRE(xs.empty());
        if (res)
          result = actor_cast<actor>(std::move(res));
      },
      [&](error&) {
        // nop
      }
    );
    if (expect_fail)
      CAF_REQUIRE(!result);
    else
      CAF_REQUIRE(result);
    return result;
  }

  actor_system_config cfg;
  union { actor_system system; }; // manually control ctor/dtor
  actor testee;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(unpublish_tests, fixture)

CAF_TEST(unpublishing) {
  CAF_EXP_THROW(port, system.middleman().publish(testee, 0));
  CAF_REQUIRE(port != 0);
  CAF_MESSAGE("published actor on port " << port);
  CAF_MESSAGE("test invalid unpublish");
  auto testee2 = system.spawn<dummy>();
  system.middleman().unpublish(testee2, port);
  auto x0 = remote_actor("127.0.0.1", port);
  CAF_CHECK_NOT_EQUAL(x0, testee2);
  CAF_CHECK_EQUAL(x0, testee);
  anon_send_exit(testee2, exit_reason::kill);
  CAF_MESSAGE("unpublish testee");
  system.middleman().unpublish(testee, port);
  CAF_MESSAGE("check whether testee is still available via cache");
  auto x1 = remote_actor("127.0.0.1", port);
  CAF_CHECK_EQUAL(x1, testee);
  CAF_MESSAGE("fake death of testee and check if testee becomes unavailable");
  anon_send(actor_cast<actor>(system.middleman().actor_handle()),
            down_msg{testee.address(), exit_reason::normal});
  // must fail now
  auto x2 = remote_actor("127.0.0.1", port, true);
  CAF_CHECK(!x2);
}

CAF_TEST_FIXTURE_SCOPE_END()
