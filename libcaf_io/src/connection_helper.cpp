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

behavior datagram_connection_broker(broker* self,
                                    uint16_t port,
                                    network::address_listing addresses,
                                    actor system_broker,
                                    basp::instance* instance) {
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
        std::vector<char> buf;
        instance->write_client_handshake(self->context(), buf,
                                         none, this_node, app_id);
        self->enqueue_datagram(hdl, std::move(buf));
        self->flush(hdl);
      }
    }
  }
  // We are not interested in attempts that do not work.
  self->set_default_handler(drop);
  return {
    [=](new_datagram_msg& msg) {
      auto hdl = msg.handle;
      auto cap = msg.buf.capacity();
      self->send(system_broker, std::move(msg), self->take(hdl), port);
      self->quit();
      // The buffer returned to the multiplexer will have capacity zero
      // if we don't do this. Handling this case here is cheaper than
      // checking before each receive in the multiplexer.
      msg.buf.reserve(cap);
    },
    after(autoconnect_timeout) >> [=]() {
      CAF_LOG_TRACE(CAF_ARG(""));
      // Nothing heard in about 10 minutes... just call it a day, then.
      CAF_LOG_INFO("aborted direct connection attempt after 10 min");
      self->quit(exit_reason::user_shutdown);
    }
  };
}

bool establish_stream_connection(stateful_actor<connection_helper_state>* self,
                                 const actor& system_broker,
                                 uint16_t port,
                                 network::address_listing& addresses) {
  auto& mx = self->system().middleman().backend();
  for (auto& kvp : addresses) {
    for (auto& addr : kvp.second) {
      auto hdl = mx.new_tcp_scribe(addr, port);
      if (hdl) {
        // Gotcha! Send scribe to our BASP broker to initiate handshake etc.
        CAF_LOG_INFO("connected directly:" << CAF_ARG(addr));
        self->send(system_broker, connect_atom::value, *hdl, port);
        return true;
      }
    }
  }
  return false;
}

behavior connection_helper(stateful_actor<connection_helper_state>* self,
                           actor system_broker, basp::instance* i) {
  CAF_LOG_TRACE(CAF_ARG(system_broker));
  self->monitor(system_broker);
  self->set_down_handler([=](down_msg& dm) {
    CAF_LOG_TRACE(CAF_ARG(dm));
    self->quit(std::move(dm.reason));
  });
  return {
    // This config is send from the remote `PeerServ`.
    [=](const std::string& item, message& msg) {
      CAF_LOG_TRACE(CAF_ARG(item) << CAF_ARG(msg));
      CAF_LOG_DEBUG("received requested config:" << CAF_ARG(msg));
      if (item.empty() || msg.empty()) {
        CAF_LOG_DEBUG("skipping empty info");
        return;
      }
      // Whatever happens, we are done afterwards.
      self->quit();
      msg.apply({
        [&](basp::routing_table::address_map& addrs) {
          if (addrs.count(network::protocol::tcp) > 0) {
            auto eps = addrs[network::protocol::tcp];
            if (!establish_stream_connection(self, system_broker, eps.first,
                                             eps.second))
              CAF_LOG_ERROR("could not connect to node ");
          }
          if (addrs.count(network::protocol::udp) > 0) {
            auto eps = addrs[network::protocol::udp];
            // Create new broker to try addresses for communication via UDP.
            auto b = self->system().middleman().spawn_broker(
              datagram_connection_broker, eps.first, std::move(eps.second),
              system_broker, i
            );
          }
        }
      });
    },
    after(autoconnect_timeout) >> [=] {
      CAF_LOG_TRACE(CAF_ARG(""));
      // Nothing heard in about 10 minutes... just a call it a day, then.
      CAF_LOG_INFO("aborted direct connection attempt after 10min");
      self->quit(exit_reason::user_shutdown);
    }
  };
}

} // namespace io
} // namespace caf

