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

#define CAF_SUITE net.actor_shell

#include "caf/net/actor_shell.hpp"

#include "net-test.hpp"

#include "caf/callback.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_transport.hpp"

using namespace caf;

namespace {

using byte_span = span<const byte>;

using svec = std::vector<std::string>;

struct app_t {
  using input_tag = tag::stream_oriented;

  template <class LowerLayer>
  error init(net::socket_manager* mgr, LowerLayer&, const settings&) {
    self = mgr->make_actor_shell();
    bhvr = behavior{
      [this](std::string& line) { lines.emplace_back(std::move(line)); },
    };
    fallback = make_type_erased_callback([](message& msg) -> result<message> {
      CAF_FAIL("unexpected message: " << msg);
      return make_error(sec::unexpected_message);
    });
    return none;
  }

  template <class LowerLayer>
  bool prepare_send(LowerLayer&) {
    while (self->consume_message(bhvr, *fallback))
      ; // Repeat.
    return true;
  }

  template <class LowerLayer>
  bool done_sending(LowerLayer&) {
    return self->try_block_mailbox();
  }

  template <class LowerLayer>
  void abort(LowerLayer&, const error& reason) {
    CAF_FAIL("app::abort called: " << reason);
  }

  template <class LowerLayer>
  ptrdiff_t consume(LowerLayer&, byte_span, byte_span) {
    CAF_FAIL("received unexpected data");
    return -1;
  }

  std::vector<std::string> lines;

  net::actor_shell_ptr self;

  behavior bhvr;

  unique_callback_ptr<result<message>(message&)> fallback;
};

struct fixture : host_fixture, test_coordinator_fixture<> {
  fixture() : mm(sys), mpx(&mm) {
    mpx.set_thread_id();
    if (auto err = mpx.init())
      CAF_FAIL("mpx.init() failed: " << err);
    auto sockets = unbox(net::make_stream_socket_pair());
    self_socket_guard.reset(sockets.first);
    testee_socket_guard.reset(sockets.second);
    if (auto err = nonblocking(testee_socket_guard.socket(), true))
      CAF_FAIL("nonblocking returned an error: " << err);
  }

  template <class Predicate>
  void run_while(Predicate predicate) {
    if (!predicate())
      return;
    for (size_t i = 0; i < 1000; ++i) {
      mpx.poll_once(false);
      if (!predicate())
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CAF_FAIL("reached max repeat rate without meeting the predicate");
  }
  net::middleman mm;
  net::multiplexer mpx;
  net::socket_guard<net::stream_socket> self_socket_guard;
  net::socket_guard<net::stream_socket> testee_socket_guard;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(actor_shell_tests, fixture)

CAF_TEST(actor shells expose their mailbox to their owners) {
  auto sck = testee_socket_guard.release();
  auto mgr = net::make_socket_manager<app_t, net::stream_transport>(sck, &mpx);
  if (auto err = mgr->init(content(cfg)))
    CAF_FAIL("mgr->init() failed: " << err);
  auto& app = mgr->top_layer();
  auto hdl = app.self.as_actor();
  anon_send(hdl, "line 1");
  anon_send(hdl, "line 2");
  anon_send(hdl, "line 3");
  run_while([&] { return app.lines.size() != 3; });
  CAF_CHECK_EQUAL(app.lines, svec({"line 1", "line 2", "line 3"}));
}

CAF_TEST_FIXTURE_SCOPE_END()
