// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/connection_helper.hpp"

#include <chrono>
#include <string>

#include "caf/actor.hpp"
#include "caf/after.hpp"
#include "caf/defaults.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/io/basp/instance.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/stateful_actor.hpp"

namespace caf::io {

namespace {

auto autoconnect_timeout = std::chrono::minutes(10);

} // namespace

behavior
connection_helper(stateful_actor<connection_helper_state>* self, actor b) {
  CAF_LOG_TRACE(CAF_ARG(b));
  self->monitor(b);
  self->set_down_handler([=](down_msg& dm) {
    CAF_LOG_TRACE(CAF_ARG(dm));
    self->quit(std::move(dm.reason));
  });
  return {
    // this config is send from the remote `ConfigServ`
    [=](const std::string& item, message& msg) {
      CAF_LOG_TRACE(CAF_ARG(item) << CAF_ARG(msg));
      CAF_LOG_DEBUG("received requested config:" << CAF_ARG(msg));
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
                  CAF_LOG_INFO("connected directly:" << CAF_ARG(addr));
                  self->send(b, connect_atom_v, *hdl, port);
                  return;
                }
              }
            }
            CAF_LOG_INFO("could not connect to node directly");
          } else {
            CAF_LOG_INFO("aborted direct connection attempt, unknown item: "
                         << CAF_ARG(item));
          }
        },
      };
      f(msg);
    },
    after(autoconnect_timeout) >>
      [=] {
        CAF_LOG_TRACE(CAF_ARG(""));
        // nothing heard in about 10 minutes... just a call it a day, then
        CAF_LOG_INFO("aborted direct connection attempt after 10min");
        self->quit(exit_reason::user_shutdown);
      },
  };
}

} // namespace caf::io
