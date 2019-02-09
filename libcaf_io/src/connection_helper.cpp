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

#include "caf/io/connection_helper.hpp"

#include <chrono>

#include "caf/defaults.hpp"
#include "caf/io/basp/instance.hpp"

namespace caf {
namespace io {

namespace {

auto autoconnect_timeout = std::chrono::minutes(10);

} // namespace <anonymous>

const char* connection_helper_state::name = "connection_helper";

behavior connection_helper(stateful_actor<connection_helper_state>* self,
                           actor b) {
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
      msg.apply({
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
                  self->send(b, connect_atom::value, *hdl, port);
                  return;
                }
              }
            }
            CAF_LOG_INFO("could not connect to node directly");
          } else {
            CAF_LOG_INFO("aborted direct connection attempt, unknown item: "
                         << CAF_ARG(item));
          }
        }
      });
    },
    after(autoconnect_timeout) >> [=] {
      CAF_LOG_TRACE(CAF_ARG(""));
      // nothing heard in about 10 minutes... just a call it a day, then
      CAF_LOG_INFO("aborted direct connection attempt after 10min");
      self->quit(exit_reason::user_shutdown);
    }
  };
}

} // namespace io
} // namespace caf

