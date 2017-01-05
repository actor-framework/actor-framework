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
#include <utility>

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

class middleman_actor_impl : public middleman_actor::base {
public:
  middleman_actor_impl(actor_config& cfg, actor default_broker)
      : middleman_actor::base(cfg),
        broker_(std::move(default_broker)) {
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

  using endpoint = std::pair<std::string, uint16_t>;

  behavior_type make_behavior() override {
    CAF_LOG_TRACE("");
    return {
      [=](publish_atom, uint16_t port, strong_actor_ptr& whom,
          mpi_set& sigs, std::string& addr, bool reuse) -> put_res {
        CAF_LOG_TRACE("");
        return put(port, whom, sigs, addr.c_str(), reuse);
      },
      [=](open_atom, uint16_t port, std::string& addr, bool reuse) -> put_res {
        CAF_LOG_TRACE("");
        strong_actor_ptr whom;
        mpi_set sigs;
        return put(port, whom, sigs, addr.c_str(), reuse);
      },
      [=](connect_atom, std::string& hostname, uint16_t port) -> get_res {
        CAF_LOG_TRACE(CAF_ARG(hostname) << CAF_ARG(port));
        auto rp = make_response_promise();
        endpoint key{std::move(hostname), port};
        // respond immediately if endpoint is cached
        auto x = cached(key);
        if (x) {
          CAF_LOG_DEBUG("found cached entry" << CAF_ARG(*x));
          rp.deliver(get<0>(*x), get<1>(*x), get<2>(*x));
          return {};
        }
        // attach this promise to a pending request if possible
        auto rps = pending(key);
        if (rps) {
          CAF_LOG_DEBUG("attach to pending request");
          rps->emplace_back(std::move(rp));
          return {};
        }
        // connect to endpoint and initiate handhsake etc.
        auto y = system().middleman().backend().new_tcp_scribe(key.first, port);
        if (!y) {
          rp.deliver(std::move(y.error()));
          return {};
        }
        auto hdl = *y;
        std::vector<response_promise> tmp{std::move(rp)};
        pending_.emplace(key, std::move(tmp));
        request(broker_, infinite, connect_atom::value, hdl, port).then(
          [=](node_id& nid, strong_actor_ptr& addr, mpi_set& sigs) {
            auto i = pending_.find(key);
            if (i == pending_.end())
              return;
            if (nid && addr) {
              monitor(addr);
              cached_.emplace(key, std::make_tuple(nid, addr, sigs));
            }
            auto res = make_message(std::move(nid), std::move(addr),
                                    std::move(sigs));
            for (auto& promise : i->second)
              promise.deliver(res);
            pending_.erase(i);
          },
          [=](error& err) {
            auto i = pending_.find(key);
            if (i == pending_.end())
              return;
            for (auto& promise : i->second)
              promise.deliver(err);
            pending_.erase(i);
          }
        );
        return {};
      },
      [=](unpublish_atom atm, actor_addr addr, uint16_t p) -> del_res {
        CAF_LOG_TRACE("");
        delegate(broker_, atm, std::move(addr), p);
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
  put_res put(uint16_t port, strong_actor_ptr& whom,
              mpi_set& sigs, const char* in = nullptr,
              bool reuse_addr = false) {
    CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(whom) << CAF_ARG(sigs)
                  << CAF_ARG(in) << CAF_ARG(reuse_addr));
    accept_handle hdl;
    uint16_t actual_port;
    // treat empty strings like nullptr
    if (in != nullptr && in[0] == '\0')
      in = nullptr;
    auto res = system().middleman().backend().new_tcp_doorman(port, in,
                                                              reuse_addr);
    if (!res)
      return std::move(res.error());
    hdl = res->first;
    actual_port = res->second;
    anon_send(broker_, publish_atom::value, hdl, actual_port,
              std::move(whom), std::move(sigs));
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
