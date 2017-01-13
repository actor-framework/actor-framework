/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include "caf/io/middleman_actor.hpp"

#include <tuple>
#include <stdexcept>

#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/actor.hpp"
#include "caf/logger.hpp"
#include "caf/node_id.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/typed_event_based_actor.hpp"

#include "caf/io/basp_broker.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/default_multiplexer.hpp"

namespace caf {
namespace io {

namespace {

// TODO: dispatching!
constexpr auto all_zeroes = "0.0.0.0";
const std::string tcp = "tcp";
const std::string udp = "udp";

} // namespace <anonymous>

namespace {

class middleman_actor_impl : public middleman_actor::base {
public:
  middleman_actor_impl(actor_config& cfg, actor default_broker)
      : middleman_actor::base(cfg),
        broker_(default_broker) {
    set_down_handler([=](down_msg& dm) {
      auto i = cached_.begin();
      auto e = cached_.end();
      while (i != e) {
        if (get<1>(i->second) == dm.source)
          i = cached_.erase(i);
        else
          ++i;
      }
    });
    set_exit_handler([=](exit_msg&) {
      // ignored, the MM links group nameservers
      // to this actor for proper shutdown ordering
    });
  }

  void on_exit() override {
    CAF_LOG_TRACE("");
    destroy(broker_);
  }

  const char* name() const override {
    return "middleman_actor";
  }

  using put_res = result<uint16_t>;

  using mpi_set = std::set<std::string>;

  using get_res = delegated<node_id, strong_actor_ptr, mpi_set>;

  using del_res = delegated<void>;

  using endpoint_data = std::tuple<node_id, strong_actor_ptr, mpi_set>;

  // using endpoint = std::pair<std::string, uint16_t>;
  using endpoint = io::uri;

  behavior_type make_behavior() override {
    CAF_SET_LOGGER_SYS(&this->system()); // TODO: remove this?
    CAF_LOG_TRACE("");
    return {
      [=](publish_atom, strong_actor_ptr& whom,
          mpi_set& sigs, uri& u, bool reuse) -> put_res {
        CAF_LOG_TRACE("");
        return put(u, whom, sigs, reuse);
      },
      [=](open_atom, uri& u, bool reuse) -> put_res {
        CAF_LOG_TRACE("");
        strong_actor_ptr whom;
        mpi_set sigs;
        return put(u, whom, sigs, reuse);
      },
      [=](connect_atom, uri u) -> get_res {
        CAF_LOG_TRACE(CAF_ARG(u));
        auto rp = make_response_promise();
        std::string host(u.host().first, u.host().second);
        auto port = u.port_as_int();
        // respond immediately if endpoint is cached
        auto x = cached(u);
        if (x) {
          CAF_LOG_DEBUG("found cached entry" << CAF_ARG(*x));
          rp.deliver(get<0>(*x), get<1>(*x), get<2>(*x));
          return {};
        }
        // attach this promise to a pending request if possible
        auto rps = pending(u);
        if (rps) {
          CAF_LOG_DEBUG("attach to pending request");
          rps->emplace_back(std::move(rp));
          return {};
        }
        if (std::distance(u.scheme().first, u.scheme().second) >= 3 &&
            equal(std::begin(tcp), std::end(tcp), u.scheme().first)) {
          // connect to endpoint and initiate handshake etc. (TCP)
          auto y = system().middleman().backend().new_tcp_scribe(host, port);
          if (!y) {
            rp.deliver(std::move(y.error()));
            return {};
          }
          auto hdl = *y;
          std::vector<response_promise> tmp{std::move(rp)};
          pending_.emplace(u, std::move(tmp));
          request(broker_, infinite, connect_atom::value, hdl, port).then(
            [=](node_id& nid, strong_actor_ptr& addr, mpi_set& sigs) {
              auto i = pending_.find(u);
              if (i == pending_.end())
                return;
              if (nid && addr) {
                monitor(addr);
                cached_.emplace(u, std::make_tuple(nid, addr, sigs));
              }
              auto res = make_message(std::move(nid), std::move(addr),
                                      std::move(sigs));
              for (auto& promise : i->second)
                promise.deliver(res);
              pending_.erase(i);
            },
            [=](error& err) {
              auto i = pending_.find(u);
              if (i == pending_.end())
                return;
              for (auto& promise : i->second)
                promise.deliver(err);
              pending_.erase(i);
            }
          );
        } else if (std::distance(u.scheme().first, u.scheme().second) >= 3 &&
                   equal(std::begin(udp), std::end(udp), u.scheme().first)) {
          // connect to endpoint and initiate handhsake etc. (UDP)
          auto y = system().middleman().backend().new_dgram_scribe(host, port);
          if (!y) {
            rp.deliver(std::move(y.error()));
            return {};
          }
          auto hdl = *y;
          std::vector<response_promise> tmp{std::move(rp)};
          pending_.emplace(u, std::move(tmp));
          request(broker_, infinite, connect_atom::value, hdl, host, port).then(
            [=](node_id& nid, strong_actor_ptr& addr, mpi_set& sigs) {
              auto i = pending_.find(u);
              if (i == pending_.end())
                return;
              if (nid && addr) {
                monitor(addr);
                cached_.emplace(u, std::make_tuple(nid, addr, sigs));
              }
              auto res = make_message(std::move(nid), std::move(addr),
                                      std::move(sigs));
              for (auto& promise : i->second)
                promise.deliver(res);
              pending_.erase(i);
            },
            [=](error& err) {
              auto i = pending_.find(u);
              if (i == pending_.end())
                return;
              for (auto& promise : i->second)
                promise.deliver(err);
              pending_.erase(i);
            }
          );
        } else {
          // sec::unsupported_protocol;
          CAF_LOG_ERROR("unsupported protocol" << CAF_ARG(u));
          std::cerr << "[MMA] Unsupported protocol '" << to_string(u.scheme())
                    << "'" << std::endl;
          rp.deliver(sec::unsupported_protocol);
        }
        return {};
      },
      [=](unpublish_atom atm, actor_addr addr, uri& u) -> del_res {
        CAF_LOG_TRACE("");
        delegate(broker_, atm, std::move(addr), u.port_as_int());
        return {};
      },
      [=](close_atom atm, uint16_t p) -> del_res {
        CAF_LOG_TRACE("");
        delegate(broker_, atm, p);
        return {};
      },
      [=](spawn_atom atm, node_id& nid, std::string& str,
          message& msg, std::set<std::string>& ifs)
      -> delegated<strong_actor_ptr> {
        CAF_LOG_TRACE("");
        delegate(broker_, forward_atom::value, nid, atom("SpawnServ"),
                 make_message(atm, std::move(str),
                              std::move(msg), std::move(ifs)));
        return {};
      },
      [=](get_atom atm, node_id nid)
      -> delegated<node_id, std::string, uint16_t> {
        CAF_LOG_TRACE("");
        delegate(broker_, atm, std::move(nid));
        return {};
      }
    };
  }

private:
  put_res put(const uri& u, strong_actor_ptr& whom,
              mpi_set& sigs, bool reuse_addr = false) {
    CAF_LOG_TRACE(CAF_ARG(u) << CAF_ARG(whom) << CAF_ARG(sigs)
                  << CAF_ARG(reuse_addr));
    uint16_t actual_port;
    // treat empty strings or 0.0.0.0 as nullptr
    auto addr = std::string(u.host().first, u.host().second);
    const char* in = nullptr;
    if ((!addr.empty() && addr.front() != '\0') || addr == all_zeroes)
      in = addr.c_str();
    if (std::distance(u.scheme().first, u.scheme().second) >= 3 &&
        equal(u.scheme().first, u.scheme().second, std::begin(tcp))) {
      // TCP
      auto res = system().middleman().backend().new_tcp_doorman(u.port_as_int(),
                                                                in, reuse_addr);
      if (!res)
        return std::move(res.error());
      accept_handle hdl = res->first;
      actual_port = res->second;
      anon_send(broker_, publish_atom::value, hdl, actual_port,
                std::move(whom), std::move(sigs));
    } else if (std::distance(u.scheme().first, u.scheme().second) >= 3 &&
               equal(u.scheme().first, u.scheme().second, std::begin(udp))) {
      /*
      // UDP
      auto res
        = system().middleman().backend().new_dgram_doorman(u.port_as_int(),
                                                           in, reuse_addr);
      if (!res)
        return std::move(res.error());
      dgram_doorman_handle hdl = res->first;
      actual_port = res->second;
      anon_send(broker_, publish_atom::value, hdl, actual_port,
                std::move(whom), std::move(sigs));
      */
    } else {
      return sec::unsupported_protocol;
    }
    return actual_port;
  }

  optional<endpoint_data&> cached(const endpoint& ep) {
    auto i = cached_.find(ep);
    if (i != cached_.end())
      return i->second;
    return none;
  }

  optional<std::vector<response_promise>&> pending(const endpoint& ep) {
    auto i = pending_.find(ep);
    if (i != pending_.end())
      return i->second;
    return none;
  }

  actor broker_;
  std::map<endpoint, endpoint_data> cached_;
  std::map<endpoint, std::vector<response_promise>> pending_;
};

} // namespace <anonymous>

middleman_actor make_middleman_actor(actor_system& sys, actor db) {
  return sys.spawn<middleman_actor_impl, detached + hidden>(std::move(db));
}

} // namespace io
} // namespace caf
