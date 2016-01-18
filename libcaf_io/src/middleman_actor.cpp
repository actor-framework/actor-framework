/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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
#include "caf/actor.hpp"
#include "caf/logger.hpp"
#include "caf/node_id.hpp"
#include "caf/exception.hpp"
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
        broker_(default_broker) {
    // nop
  }

  void on_exit() override {
    CAF_LOG_TRACE("");
    broker_ = invalid_actor;
  }

  const char* name() const override {
    return "middleman_actor";
  }

  using put_res = maybe<std::tuple<ok_atom, uint16_t>>;

  using mpi_set = std::set<std::string>;

  using get_res = delegated<ok_atom, node_id, actor_addr, mpi_set>;

  using del_res = delegated<void>;

  using endpoint_data = std::tuple<node_id, actor_addr, mpi_set>;

  using endpoint = std::pair<std::string, uint16_t>;

  behavior_type make_behavior() override {
    CAF_LOG_TRACE("");
    return {
      [=](publish_atom, uint16_t port, actor_addr& whom,
          mpi_set& sigs, std::string& addr, bool reuse) {
        CAF_LOG_TRACE("");
        return put(port, whom, sigs, addr.c_str(), reuse);
      },
      [=](open_atom, uint16_t port, std::string& addr, bool reuse) -> put_res {
        CAF_LOG_TRACE("");
        actor_addr whom = invalid_actor_addr;
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
          rp.deliver(ok_atom::value, get<0>(*x), get<1>(*x), get<2>(*x));
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
        connection_handle hdl;
        try {
          hdl = system().middleman().backend().new_tcp_scribe(key.first, port);
        } catch(std::exception&) {
          rp.deliver(sec::cannot_connect_to_node);
          return {};
        }
        std::vector<response_promise> tmp{std::move(rp)};
        pending_.emplace(key, std::move(tmp));
        request(broker_, connect_atom::value, hdl, port).then(
          [=](ok_atom, node_id& nid, actor_addr& addr, mpi_set& sigs) {
            auto i = pending_.find(key);
            if (i == pending_.end())
              return;
            monitor(addr);
            cached_.emplace(key, std::make_tuple(nid, addr, sigs));
            auto res = make_message(ok_atom::value, std::move(nid),
                                    std::move(addr), std::move(sigs));
            for (auto& x : i->second)
              x.deliver(res);
            pending_.erase(i);
          },
          [=](error& err) {
            auto i = pending_.find(key);
            if (i == pending_.end())
              return;
            for (auto& x : i->second)
              x.deliver(err);
            pending_.erase(i);
          }
        );
        return {};
      },
      [=](unpublish_atom, const actor_addr&, uint16_t) -> del_res {
        CAF_LOG_TRACE("");
        forward_current_message(broker_);
        return {};
      },
      [=](close_atom, uint16_t) -> del_res {
        CAF_LOG_TRACE("");
        forward_current_message(broker_);
        return {};
      },
      [=](spawn_atom, const node_id&, const std::string&, const message&)
      -> delegated<ok_atom, actor_addr, mpi_set> {
        CAF_LOG_TRACE("");
        forward_current_message(broker_);
        return {};
      },
      [=](const down_msg& dm) {
        auto i = cached_.begin();
        auto e = cached_.end();
        while (i != e) {
          if (get<1>(i->second) == dm.source)
            i = cached_.erase(i);
          else
            ++i;
        }
      }
    };
  }

private:
  put_res put(uint16_t port, actor_addr& whom,
              mpi_set& sigs, const char* in = nullptr,
              bool reuse_addr = false) {
    CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(whom) << CAF_ARG(sigs)
                  << CAF_ARG(in) << CAF_ARG(reuse_addr));
    accept_handle hdl;
    uint16_t actual_port;
    // treat empty strings like nullptr
    if (in != nullptr && in[0] == '\0')
      in = nullptr;
    try {
      auto res = system().middleman().backend().new_tcp_doorman(port, in,
                                                                reuse_addr);
      hdl = res.first;
      actual_port = res.second;
      send(broker_, publish_atom::value, hdl, actual_port,
           std::move(whom), std::move(sigs));
      return {ok_atom::value, actual_port};
    }
    catch (std::exception&) {
      return sec::cannot_open_port;
    }
  }

  maybe<endpoint_data&> cached(const endpoint& ep) {
    auto i = cached_.find(ep);
    if (i != cached_.end())
      return i->second;
    return none;
  }

  maybe<std::vector<response_promise>&> pending(const endpoint& ep) {
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
