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

behavior datagram_connection_broker(broker* self, uint16_t port,
                                    network::address_listing addresses,
                                    actor system_broker) {
  auto& mx = self->system().middleman().backend();
  auto& this_node = self->system().node();
  auto app_id = get_or(self->config(), "middleman.app-identifier",
                       defaults::middleman::app_identifier);
  for (auto& kvp : addresses) {
    for (auto& addr : kvp.second) {
      auto eptr = mx.new_remote_udp_endpoint(addr, port);
      if (eptr) {
        auto hdl = (*eptr)->hdl();
        self->add_datagram_servant(std::move(*eptr));
        basp::instance::write_client_handshake(self->context(),
                                               self->wr_buf(hdl),
                                               none, this_node,
                                               app_id);
      }
    }
  }
  return {
    [=](new_datagram_msg& msg) {
      auto hdl = msg.handle;
      self->send(system_broker, std::move(msg), self->take(hdl), port);
      self->quit();
    },
    after(autoconnect_timeout) >> [=]() {
      CAF_LOG_TRACE(CAF_ARG(""));
      // nothing heard in about 10 minutes... just a call it a day, then
      CAF_LOG_INFO("aborted direct connection attempt after 10min");
      self->quit(exit_reason::user_shutdown);
    }
  };
}

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
          } else if (item == "basp.default-connectivity-udp") {
            auto& sys = self->system();
            // create new broker to try addresses for communication via UDP
            if (get_or(sys.config(), "middleman.attach-utility-actors", false))
              self->system().middleman().spawn_broker<hidden>(
                datagram_connection_broker, port, std::move(addresses), b
              );
            else
              self->system().middleman().spawn_broker<detached + hidden>(
                datagram_connection_broker, port, std::move(addresses), b
              );
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

