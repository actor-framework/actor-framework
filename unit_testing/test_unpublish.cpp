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

#include <thread>
#include <atomic>

#include "test.hpp"

#include "caf/all.hpp"
#include "caf/io/all.hpp"

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
      others() >> CAF_UNEXPECTED_MSG_CB(this)
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

void test_unpublish() {
  auto d = spawn<dummy>();
  auto port = io::publish(d, 0);
  CAF_CHECKPOINT();
  test_invalid_unpublish(d, port);
  CAF_CHECKPOINT();
  io::unpublish(d, port);
  CAF_CHECKPOINT();
  // must fail now
  try {
    io::remote_actor("127.0.0.1", port);
    CAF_FAILURE("unexpected: remote actor succeeded!");
  } catch (network_error&) {
    CAF_CHECKPOINT();
  }
  anon_send_exit(d, exit_reason::user_shutdown);
}

} // namespace <anonymous>

int main() {
  CAF_TEST(test_unpublish);
  test_unpublish();
  await_all_actors_done();
  shutdown();
  CAF_CHECK_EQUAL(s_dtor_called.load(), 2);
  return CAF_TEST_RESULT();
}
