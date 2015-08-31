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
  ~dummy() {
    ++s_dtor_called;
  }
  behavior make_behavior() override {
    return {
      others >> [&] {
        CAF_TEST_ERROR("Unexpected message: " << to_string(current_message()));
      }
    };
  }
};

void test_invalid_unpublish(const actor& published, uint16_t port) {
  auto d = spawn<dummy>();
  io::unpublish(d, port);
  auto ra = io::remote_actor("127.0.0.1", port);
  CAF_CHECK(ra != d);
  CAF_CHECK(ra == published);
  anon_send_exit(d, exit_reason::user_shutdown);
}

CAF_TEST(unpublishing) {
  auto argv = test::engine::argv();
  auto argc = test::engine::argc();
  if (argc == 1 && strcmp(argv[0], "--use-asio") == 0) {
#   ifdef CAF_USE_ASIO
    CAF_MESSAGE("enable ASIO backend");
    io::set_middleman<io::network::asio_multiplexer>();
#   endif // CAF_USE_ASIO
  }
  { // scope for local variables
    auto d = spawn<dummy>();
    auto port = io::publish(d, 0);
    CAF_MESSAGE("published actor on port " << port);
    test_invalid_unpublish(d, port);
    CAF_MESSAGE("finished `invalid_unpublish`");
    io::unpublish(d, port);
    // must fail now
    try {
      CAF_MESSAGE("expect exception...");
      io::remote_actor("127.0.0.1", port);
      CAF_TEST_ERROR("unexpected: remote actor succeeded!");
    } catch (network_error&) {
      CAF_MESSAGE("unpublish succeeded");
    }
    anon_send_exit(d, exit_reason::user_shutdown);
  }
  await_all_actors_done();
  shutdown();
  CAF_CHECK_EQUAL(s_dtor_called.load(), 2);
}

} // namespace <anonymous>
