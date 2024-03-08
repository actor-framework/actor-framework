// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/connection_helper.hpp"

#include "caf/io/basp/instance.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/network/interfaces.hpp"

#include "caf/actor.hpp"
#include "caf/after.hpp"
#include "caf/defaults.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/log/io.hpp"
#include "caf/stateful_actor.hpp"

#include <chrono>
#include <string>

namespace caf::io {

namespace {

auto autoconnect_timeout = std::chrono::minutes(10);

} // namespace

behavior connection_helper(stateful_actor<connection_helper_state>* self,
                           actor b) {
  auto lg = log::io::trace("b = {}", b);
  self->monitor(b, [=](error reason) {
    auto lg = log::io::trace("dm = {}", reason);
    self->quit(std::move(reason));
  });
  return {
    // this config is send from the remote `ConfigServ`
    [=](const std::string& item, message& msg) {
      auto lg = log::io::trace("item = {}, msg = {}", item, msg);
      log::io::debug("received requested config: msg = {}", msg);
      // whatever happens, we are done afterwards
      self->quit();
      message_handler f{
        [&](uint16_t port, network::address_listing& addresses) {
          if (item == "basp.default-connectivity-tcp") {
            auto& mx = self->system().middleman().backend();
            for (auto& kvp : addresses) {
              for (auto& addr : kvp.second) {
                auto hdl = mx.new_tcp_scribe(addr, port);
                if (hdl) {
                  // gotcha! send scribe to our BASP broker
                  // to initiate handshake etc.
                  log::io::info("connected directly: addr = {}", addr);
                  self->mail(connect_atom_v, *hdl, port).send(b);
                  return;
                }
              }
            }
            log::io::info("could not connect to node directly");
          } else {
            log::io::info(
              "aborted direct connection attempt, unknown item: item = {}",
              item);
          }
        },
      };
      f(msg);
    },
    after(autoconnect_timeout) >>
      [=] {
        auto lg = log::io::trace("");
        // nothing heard in about 10 minutes... just a call it a day, then
        log::io::info("aborted direct connection attempt after 10min");
        self->quit(exit_reason::user_shutdown);
      },
  };
}

} // namespace caf::io
