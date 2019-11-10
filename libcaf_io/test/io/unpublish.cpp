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

#define CAF_SUITE io.unpublish

#include "caf/config.hpp"

#include "caf/test/io_dsl.hpp"

#include <atomic>
#include <memory>
#include <new>
#include <thread>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

struct suite_state {
  long dtors_called = 0;
  suite_state() = default;
};

using suite_state_ptr = std::shared_ptr<suite_state>;

class dummy : public event_based_actor {
public:
  dummy(actor_config& cfg, suite_state_ptr ssp)
    : event_based_actor(cfg), ssp_(std::move(ssp)) {
    // nop
  }

  ~dummy() override {
    ssp_->dtors_called += 1;
  }

  behavior make_behavior() override {
    return {[] {
      // nop
    }};
  }

private:
  suite_state_ptr ssp_;
};

struct fixture : point_to_point_fixture<> {
  fixture() {
    prepare_connection(mars, earth, "mars", 8080);
    ssp = std::make_shared<suite_state>();
  }

  ~fixture() {
    run();
    CAF_CHECK_EQUAL(ssp->dtors_called, 2);
  }

  suite_state_ptr ssp;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(unpublish_tests, fixture)

CAF_TEST(actors can become unpublished) {
  auto testee = mars.sys.spawn<dummy>(ssp);
  auto guard = detail::make_scope_guard([&] {
    // The MM holds a reference to this actor publishing it, so we need to kill
    // it manually.
    anon_send_exit(testee, exit_reason::user_shutdown);
  });
  loop_after_next_enqueue(mars);
  auto port = unbox(mars.mm.publish(testee, 8080));
  CAF_REQUIRE_EQUAL(port, 8080);
  CAF_MESSAGE("the middleman ignores invalid unpublish() calls");
  auto testee2 = mars.sys.spawn<dummy>(ssp);
  loop_after_next_enqueue(mars);
  auto res = mars.mm.unpublish(testee2, 8080);
  CAF_CHECK(!res && res.error() == sec::no_actor_published_at_port);
  anon_send_exit(testee2, exit_reason::user_shutdown);
  CAF_MESSAGE("after unpublishing an actor, remotes can no longer connect");
  loop_after_next_enqueue(mars);
  CAF_CHECK(mars.mm.unpublish(testee, 8080));
  // TODO: ideally, we'd check that remote actors in fact can no longer connect.
  //       However, the test multiplexer does not support "closing" connections
  //       and the remote_actor blocks forever.
  // run();
  // loop_after_next_enqueue(earth);
  // CAF_CHECK(!earth.mm.remote_actor("mars", 8080));
}

CAF_TEST_FIXTURE_SCOPE_END()
