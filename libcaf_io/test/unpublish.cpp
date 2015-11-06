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

#include <thread>
#include <atomic>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#ifdef CAF_USE_ASIO
#include "caf/io/network/asio_multiplexer.hpp"
#endif // CAF_USE_ASIO

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
        CAF_TEST_ERROR("Unexpected message");
      }
    };
  }
};

void test_invalid_unpublish(actor_system& system, const actor& published,
                            uint16_t port) {
  auto d = system.spawn<dummy>();
  system.middleman().unpublish(d, port);
  auto ra = system.middleman().remote_actor("127.0.0.1", port);
  CAF_REQUIRE(ra);
  CAF_CHECK(ra != d);
  CAF_CHECK(ra == published);
  anon_send_exit(d, exit_reason::user_shutdown);
}

CAF_TEST(unpublishing) {
  actor_system_config cfg;
# ifdef CAF_USE_ASIO
  auto argc = test::engine::argc();
  auto argv = test::engine::argv();
  if (argc == 1 && strcmp(argv[0], "--use-asio") == 0)
    cfg.load<io::middleman, io::network::asio_multiplexer>());
  else
# endif // CAF_USE_ASIO
    cfg.load<io::middleman>();
  { // scope for local variables
    actor_system system{cfg};
    auto d = system.spawn<dummy>();
    auto port = system.middleman().publish(d, 0);
    CAF_REQUIRE(port);
    CAF_MESSAGE("published actor on port " << *port);
    test_invalid_unpublish(system, d, *port);
    CAF_MESSAGE("finished `invalid_unpublish`");
    system.middleman().unpublish(d, *port);
    // must fail now
    CAF_MESSAGE("expect error...");
    try {
      auto res = system.middleman().remote_actor("127.0.0.1", *port);
      CAF_TEST_ERROR("unexpected: remote actor succeeded!");
    } catch(std::exception&) {
      CAF_MESSAGE("unpublish succeeded");
    }
    anon_send_exit(d, exit_reason::user_shutdown);
    system.await_all_actors_done();
  }
  // check after dtor of system was called
  CAF_CHECK_EQUAL(s_dtor_called.load(), 2);
}

} // namespace <anonymous>
