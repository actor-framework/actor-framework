// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/middleman_actor_impl.hpp"

#include "caf/io/basp/header.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/interfaces.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/actor.hpp"
#include "caf/actor_proxy.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/logger.hpp"
#include "caf/node_id.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <stdexcept>
#include <tuple>
#include <utility>

namespace caf::io {

middleman_actor_impl::middleman_actor_impl(actor_config& cfg,
                                           actor default_broker)
  : middleman_actor::base(cfg), broker_(std::move(default_broker)) {
  set_down_handler([this](down_msg& dm) {
    auto i = cached_tcp_.begin();
    auto e = cached_tcp_.end();
    while (i != e) {
      if (get<1>(i->second) == dm.source)
        i = cached_tcp_.erase(i);
      else
        ++i;
    }
  });
  set_exit_handler([=](exit_msg&) {
    // ignored, the MM links group nameservers
    // to this actor for proper shutdown ordering
  });
}

void middleman_actor_impl::on_exit() {
  CAF_LOG_TRACE("");
  broker_ = nullptr;
  cached_tcp_.clear();
  for (auto& kvp : pending_)
    for (auto& promise : kvp.second)
      promise.deliver(make_error(sec::cannot_connect_to_node));
  pending_.clear();
}

const char* middleman_actor_impl::name() const {
  return "caf.system.middleman-actor";
}

auto middleman_actor_impl::make_behavior() -> behavior_type {
  CAF_LOG_TRACE("");
  auto res = behavior{
    [this](publish_atom, uint16_t port, strong_actor_ptr& whom, mpi_set& sigs,
           std::string& addr, bool reuse) -> put_res {
      CAF_LOG_TRACE("");
      return put(port, whom, sigs, addr.c_str(), reuse);
    },
    [this](open_atom, uint16_t port, std::string& addr, bool reuse) -> put_res {
      CAF_LOG_TRACE("");
      strong_actor_ptr whom;
      mpi_set sigs;
      return put(port, whom, sigs, addr.c_str(), reuse);
    },
    [this](delete_atom, std::string& hostname, uint16_t port) {
      // Undocumented (on purpose): manually removes an entry from the cache.
      CAF_LOG_TRACE(CAF_ARG(hostname) << CAF_ARG(port));
      cached_tcp_.erase(endpoint{std::move(hostname), port});
    },
    [this](connect_atom, std::string& hostname, uint16_t port) -> get_res {
      CAF_LOG_TRACE(CAF_ARG(hostname) << CAF_ARG(port));
      auto rp = make_response_promise();
      endpoint key{std::move(hostname), port};
      // respond immediately if endpoint is cached
      auto x = cached_tcp(key);
      if (x) {
        CAF_LOG_DEBUG("found cached entry" << CAF_ARG(*x));
        rp.deliver(get<0>(*x), get<1>(*x), get<2>(*x));
        return get_delegated{};
      }
      // attach this promise to a pending request if possible
      auto rps = pending(key);
      if (rps) {
        CAF_LOG_DEBUG("attach to pending request");
        rps->emplace_back(std::move(rp));
        return get_delegated{};
      }
      // connect to endpoint and initiate handhsake etc.
      auto r = connect(key.first, port);
      if (!r) {
        rp.deliver(std::move(r.error()));
        return get_delegated{};
      }
      auto& ptr = *r;
      std::vector<response_promise> tmp{std::move(rp)};
      pending_.emplace(key, std::move(tmp));
      request(broker_, infinite, connect_atom_v, std::move(ptr), port)
        .then(
          [this, key](node_id& nid, strong_actor_ptr& addr, mpi_set& sigs) {
            auto i = pending_.find(key);
            if (i == pending_.end())
              return;
            if (nid && addr) {
              monitor(addr);
              cached_tcp_.emplace(key, std::make_tuple(nid, addr, sigs));
            }
            auto res = make_message(std::move(nid), std::move(addr),
                                    std::move(sigs));
            for (auto& promise : i->second)
              promise.deliver(res);
            pending_.erase(i);
          },
          [this, key](error& err) {
            auto i = pending_.find(key);
            if (i == pending_.end())
              return;
            for (auto& promise : i->second)
              promise.deliver(err);
            pending_.erase(i);
          });
      return get_delegated{};
    },
    [this](unpublish_atom atm, actor_addr addr, uint16_t p) -> del_res {
      CAF_LOG_TRACE("");
      delegate(broker_, atm, std::move(addr), p);
      return {};
    },
    [this](close_atom atm, uint16_t p) -> del_res {
      CAF_LOG_TRACE("");
      delegate(broker_, atm, p);
      return {};
    },
    [this](spawn_atom, node_id& nid, std::string& name, message& args,
           std::set<std::string>& ifs) -> result<strong_actor_ptr> {
      CAF_LOG_TRACE("");
      if (!nid)
        return make_error(sec::invalid_argument,
                          "cannot spawn actors on invalid nodes");
      if (name.empty())
        return make_error(sec::invalid_argument,
                          "cannot spawn actors without a type name");
      if (nid == system().node()) {
        if (auto res = system().spawn<actor>(name, std::move(args), nullptr,
                                             true, &ifs))
          return actor_cast<strong_actor_ptr>(std::move(*res));
        else
          return std::move(res.error());
      }
      // This local variable prevents linker errors (delegate forms an lvalue
      // reference but spawn_server_id is constexpr).
      auto id = basp::header::spawn_server_id;
      delegate(broker_, forward_atom_v, nid, id,
               make_message(spawn_atom_v, std::move(name), std::move(args),
                            std::move(ifs)));
      return delegated<strong_actor_ptr>{};
    },
    [this](get_atom, group_atom, node_id& nid,
           std::string& group_id) -> result<actor> {
      CAF_LOG_TRACE("");
      if (!nid)
        return make_error(sec::invalid_argument,
                          "cannot get group intermediaries from invalid nodes");
      auto id = basp::header::config_server_id;
      delegate(broker_, forward_atom_v, nid, id,
               make_message(get_atom_v, group_atom_v, std::move(nid),
                            std::move(group_id)));
      return delegated<actor>{};
    },
    [this](get_atom,
           node_id& nid) -> delegated<node_id, std::string, uint16_t> {
      CAF_LOG_TRACE("");
      delegate(broker_, get_atom_v, std::move(nid));
      return {};
    },
  };
  // Note: needed to sneak the extra `delete_atom` handler into the behavior.
  return behavior_type{unsafe_behavior_init, std::move(res)};
}

middleman_actor_impl::put_res
middleman_actor_impl::put(uint16_t port, strong_actor_ptr& whom, mpi_set& sigs,
                          const char* in, bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(whom) << CAF_ARG(sigs) << CAF_ARG(in)
                              << CAF_ARG(reuse_addr));
  uint16_t actual_port;
  // treat empty strings like nullptr
  if (in != nullptr && in[0] == '\0')
    in = nullptr;
  auto res = open(port, in, reuse_addr);
  if (!res)
    return std::move(res.error());
  auto& ptr = *res;
  actual_port = ptr->port();
  anon_send(broker_, publish_atom_v, std::move(ptr), actual_port,
            std::move(whom), std::move(sigs));
  return actual_port;
}

middleman_actor_impl::put_res
middleman_actor_impl::put_udp(uint16_t port, strong_actor_ptr& whom,
                              mpi_set& sigs, const char* in, bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(whom) << CAF_ARG(sigs) << CAF_ARG(in)
                              << CAF_ARG(reuse_addr));
  uint16_t actual_port;
  // treat empty strings like nullptr
  if (in != nullptr && in[0] == '\0')
    in = nullptr;
  auto res = open_udp(port, in, reuse_addr);
  if (!res)
    return std::move(res.error());
  auto& ptr = *res;
  actual_port = ptr->local_port();
  anon_send(broker_, publish_udp_atom_v, std::move(ptr), actual_port,
            std::move(whom), std::move(sigs));
  return actual_port;
}

middleman_actor_impl::endpoint_data*
middleman_actor_impl::cached_tcp(const endpoint& ep) {
  auto i = cached_tcp_.find(ep);
  if (i != cached_tcp_.end())
    return std::addressof(i->second);
  return nullptr;
}

middleman_actor_impl::endpoint_data*
middleman_actor_impl::cached_udp(const endpoint& ep) {
  auto i = cached_udp_.find(ep);
  if (i != cached_udp_.end())
    return std::addressof(i->second);
  return nullptr;
}

std::vector<response_promise>*
middleman_actor_impl::pending(const endpoint& ep) {
  auto i = pending_.find(ep);
  if (i != pending_.end())
    return std::addressof(i->second);
  return nullptr;
}

expected<scribe_ptr> middleman_actor_impl::connect(const std::string& host,
                                                   uint16_t port) {
  return system().middleman().backend().new_tcp_scribe(host, port);
}

expected<datagram_servant_ptr>
middleman_actor_impl::contact(const std::string& host, uint16_t port) {
  return system().middleman().backend().new_remote_udp_endpoint(host, port);
}

expected<doorman_ptr> middleman_actor_impl::open(uint16_t port,
                                                 const char* addr, bool reuse) {
  return system().middleman().backend().new_tcp_doorman(port, addr, reuse);
}

expected<datagram_servant_ptr>
middleman_actor_impl::open_udp(uint16_t port, const char* addr, bool reuse) {
  return system().middleman().backend().new_local_udp_endpoint(port, addr,
                                                               reuse);
}

} // namespace caf::io
