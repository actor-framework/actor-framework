// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/lp/with.hpp"

#include "caf/test/test.hpp"

#include "caf/net/middleman.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/chunk.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/observable_builder.hpp"

using namespace caf;

namespace {

namespace lp = caf::net::lp;

} // namespace

TEST("LP acceptor memory leak regression") {
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
  auto acceptor = unbox(net::make_tcp_accept_socket(0));
  auto maybe_hdl = lp::with(sys) //
                     .accept(acceptor)
                     .start([&sys](auto events) {
                       sys.spawn([events](event_based_actor* self) {
                         self->make_observable().from_resource(events).for_each(
                           [self](const auto& ev) {
                             auto [pull, push] = ev.data();
                             pull.observe_on(self).subscribe(push);
                           });
                       });
                     });
  auto server_hdl = unbox(maybe_hdl);
  // Do not run the multiplexer after disposal. The acceptor must clean up its
  // references while the actor system is shutting down. Otherwise, this test
  // triggers a memory leak (under ASan/LeakSan).
  server_hdl.dispose();
}
