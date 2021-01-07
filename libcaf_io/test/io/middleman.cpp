// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

// Note: this suite is disabled via CMake on Windows, because it lacks the
//       socketpair() function.

#define CAF_SUITE io.middleman

#include "caf/io/middleman.hpp"

#include "caf/test/io_dsl.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include "caf/actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/behavior.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/scribe_impl.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

namespace {

// Unlike our usual fixtures, this test suite does *not* use the test
// coordinator.
struct node_fixture {
  struct config : actor_system_config {
    config() {
      load<io::middleman>();
      set("caf.scheduler.policy", "sharing");
      set("caf.scheduler.max-threads", 1);
      set("caf.middleman.workers", 0);
    }
  };

  node_fixture() : sys(cfg), mm(sys.middleman()), mpx(mm.backend()), self(sys) {
    basp_broker = mm.get_named_broker("BASP");
  }

  config cfg;
  actor_system sys;
  io::middleman& mm;
  io::network::multiplexer& mpx;
  scoped_actor self;
  actor basp_broker;
};

struct fixture {
  node_fixture earth;
  node_fixture mars;

  fixture() {
    // Connect the two BASP brokers via connected socket pair.
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
      CAF_FAIL("socketpair failed");
    io::network::nonblocking(sockets[0], true);
    io::network::nonblocking(sockets[1], true);
    auto server_scribe = earth.mpx.new_scribe(sockets[0]);
    auto client_scribe = mars.mpx.new_scribe(sockets[1]);
    anon_send(earth.basp_broker, publish_atom_v, std::move(server_scribe),
              uint16_t(4242), strong_actor_ptr{}, std::set<std::string>{});
    mars.self
      ->request(mars.basp_broker, std::chrono::minutes(1), connect_atom_v,
                std::move(client_scribe), uint16_t(4242))
      .receive(
        [&](node_id& nid, strong_actor_ptr&, std::set<std::string>&) {
          if (nid != earth.sys.node())
            CAF_FAIL("mars failed to connect to earth: unexpected node ID");
        },
        [&](const caf::error& err) {
          CAF_FAIL("mars failed to connect to earth: " << err);
        });
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(middleman_tests, fixture)

CAF_TEST(remote_lookup allows registry lookups on other nodes) {
  auto testee_impl = []() -> behavior {
    return {
      [](int32_t x, int32_t y) { return x + y; },
    };
  };
  auto testee = earth.sys.spawn(testee_impl);
  earth.sys.registry().put("testee", testee);
  auto testee_proxy_ptr = mars.mm.remote_lookup("testee", earth.sys.node());
  auto testee_proxy = actor_cast<actor>(testee_proxy_ptr);
  CAF_CHECK_EQUAL(testee->node(), testee_proxy.node());
  CAF_CHECK_EQUAL(testee->id(), testee_proxy.id());
  mars.self
    ->request(testee_proxy, std::chrono::minutes(1), int32_t{7}, int32_t{8})
    .receive([](int32_t result) { CAF_CHECK_EQUAL(result, 15); },
             [](caf::error& err) { CAF_FAIL("request failed: " << err); });
  anon_send_exit(testee, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
