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

#include "caf/io/basp_broker.hpp"

#include <limits>
#include <chrono>

#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/after.hpp"
#include "caf/make_counted.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/forwarding_actor_proxy.hpp"

#include "caf/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/io/basp/all.hpp"
#include "caf/io/middleman.hpp"

#include "caf/io/network/interfaces.hpp"

namespace caf {
namespace io {

const char* basp_broker_state::name = "basp_broker";

/******************************************************************************
 *                             basp_broker_state                              *
 ******************************************************************************/

basp_broker_state::basp_broker_state(broker* selfptr)
    : basp::instance::callee(selfptr->system(),
                             static_cast<proxy_registry::backend&>(*this)),
      self(selfptr),
      instance(selfptr, *this) {
  CAF_ASSERT(this_node() != invalid_node_id);
}

basp_broker_state::~basp_broker_state() {
  // make sure all spawn servers are down
  for (auto& kvp : spawn_servers)
    anon_send_exit(kvp.second, exit_reason::kill);
}

strong_actor_ptr basp_broker_state::make_proxy(node_id nid, actor_id aid) {
  CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(aid));
  CAF_ASSERT(nid != this_node());
  if (nid == invalid_node_id || aid == invalid_actor_id)
    return nullptr;
  // this member function is being called whenever we deserialize a
  // payload received from a remote node; if a remote node A sends
  // us a handle to a third node B, then we assume that A offers a route to B
  if (nid != this_context->id
      && instance.tbl().lookup_direct(nid) == invalid_connection_handle
      && instance.tbl().add_indirect(this_context->id, nid))
    learned_new_node_indirectly(nid);
  // we need to tell remote side we are watching this actor now;
  // use a direct route if possible, i.e., when talking to a third node
  auto path = instance.tbl().lookup(nid);
  if (! path) {
    // this happens if and only if we don't have a path to `nid`
    // and current_context_->hdl has been blacklisted
    CAF_LOG_INFO("cannot create a proxy instance for an actor "
                 "running on a node we don't have a route to");
    return nullptr;
  }
  // create proxy and add functor that will be called if we
  // receive a kill_proxy_instance message
  auto mm = &system().middleman();
  actor_config cfg;
  auto res = make_actor<forwarding_actor_proxy, strong_actor_ptr>(
    aid, nid, &(self->home_system()), cfg, self);
  strong_actor_ptr selfptr{self->ctrl()};
  res->get()->attach_functor([=](const error& rsn) {
    mm->backend().post([=] {
      // using res->id() instead of aid keeps this actor instance alive
      // until the original instance terminates, thus preventing subtle
      // bugs with attachables
      auto bptr = static_cast<basp_broker*>(selfptr->get());
      if (! bptr->is_terminated())
        bptr->state.proxies().erase(nid, res->id(), rsn);
    });
  });
  CAF_LOG_INFO("successfully created proxy instance, "
               "write announce_proxy_instance:"
               << CAF_ARG(nid) << CAF_ARG(aid));
  // tell remote side we are monitoring this actor now
  instance.write_announce_proxy(self->context(),
                                self->wr_buf(this_context->hdl), nid, aid);
  instance.tbl().flush(*path);
  mm->notify<hook::new_remote_actor>(res);
  return res;
}

execution_unit* basp_broker_state::registry_context() {
  return self->context();
}

void basp_broker_state::finalize_handshake(const node_id& nid, actor_id aid,
                                           std::set<std::string>& sigs) {
  CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(aid) << CAF_ARG(sigs));
  CAF_ASSERT(this_context != nullptr);
  this_context->id = nid;
  auto& cb = this_context->callback;
  if (! cb)
    return;
  auto cleanup = detail::make_scope_guard([&] {
    cb = none;
  });
  strong_actor_ptr ptr;
  if (aid == invalid_actor_id) {
    // can occur when connecting to the default port of a node
    cb->deliver(ok_atom::value, nid, ptr, std::move(sigs));
    return;
  }
  if (nid == this_node()) {
    // connected to self
    ptr = actor_cast<strong_actor_ptr>(system().registry().get(aid));
    CAF_LOG_INFO_IF(! ptr, "actor not found:" << CAF_ARG(aid));
  } else {
    ptr = namespace_.get_or_put(nid, aid);
    CAF_LOG_ERROR_IF(! ptr, "creating actor in finalize_handshake failed");
  }
  cb->deliver(make_message(ok_atom::value, nid, ptr, std::move(sigs)));
  this_context->callback = none;
}

void basp_broker_state::purge_state(const node_id& nid) {
  CAF_LOG_TRACE(CAF_ARG(nid));
  auto hdl = instance.tbl().lookup_direct(nid);
  if (hdl == invalid_connection_handle)
    return;
  auto i = ctx.find(hdl);
  if (i != ctx.end()) {
    auto& ref = i->second;
    if (ref.callback) {
      CAF_LOG_DEBUG("connection closed during handshake");
      ref.callback->deliver(sec::disconnect_during_handshake);
    }
    ctx.erase(i);
  }
  proxies().erase(nid);
}

void basp_broker_state::proxy_announced(const node_id& nid, actor_id aid) {
  CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(aid));
  // source node has created a proxy for one of our actors
  auto entry = system().registry().get(aid);
  auto send_kill_proxy_instance = [=](error rsn) {
    if (! rsn)
      rsn = exit_reason::unknown;
    auto path = instance.tbl().lookup(nid);
    if (! path) {
      CAF_LOG_INFO("cannot send exit message for proxy, no route to host:"
                   << CAF_ARG(nid));
      return;
    }
    instance.write_kill_proxy(self->context(), path->wr_buf,
                                       nid, aid, rsn);
    instance.tbl().flush(*path);
  };
  auto ptr = actor_cast<strong_actor_ptr>(entry);
  if (! ptr) {
    CAF_LOG_DEBUG("kill proxy immediately");
    // kill immediately if actor has already terminated
    send_kill_proxy_instance(exit_reason::unknown);
  } else {
    strong_actor_ptr tmp{self->ctrl()};
    auto mm = &system().middleman();
    ptr->get()->attach_functor([=](const error& fail_state) {
      mm->backend().dispatch([=] {
        CAF_LOG_TRACE(CAF_ARG(fail_state));
        auto bptr = static_cast<basp_broker*>(tmp->get());
        // ... to make sure this is safe
        if (bptr == mm->named_broker<basp_broker>(atom("BASP"))
            && ! bptr->is_terminated())
          send_kill_proxy_instance(fail_state);
      });
    });
  }
}

void basp_broker_state::kill_proxy(const node_id& nid, actor_id aid,
                                   const error& rsn) {
  CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(aid) << CAF_ARG(rsn));
  proxies().erase(nid, aid, rsn);
}

void basp_broker_state::deliver(const node_id& src_nid, actor_id src_aid,
                                actor_id dest_aid, message_id mid,
                                std::vector<strong_actor_ptr>& stages,
                                message& msg) {
  CAF_LOG_TRACE(CAF_ARG(src_nid) << CAF_ARG(src_aid)
                << CAF_ARG(dest_aid) << CAF_ARG(msg) << CAF_ARG(mid));
  deliver(src_nid, src_aid, system().registry().get(dest_aid),
          mid, stages, msg);
}

void basp_broker_state::deliver(const node_id& src_nid, actor_id src_aid,
                                atom_value dest_name, message_id mid,
                                std::vector<strong_actor_ptr>& stages,
                                message& msg) {
  CAF_LOG_TRACE(CAF_ARG(src_nid) << CAF_ARG(src_aid)
                << CAF_ARG(dest_name) << CAF_ARG(msg) << CAF_ARG(mid));
  deliver(src_nid, src_aid, system().registry().get(dest_name),
          mid, stages, msg);
}

void basp_broker_state::deliver(const node_id& src_nid, actor_id src_aid,
                                strong_actor_ptr dest, message_id mid,
                                std::vector<strong_actor_ptr>& stages,
                                message& msg) {
  CAF_LOG_TRACE(CAF_ARG(src_nid) << CAF_ARG(src_aid) << CAF_ARG(dest)
                << CAF_ARG(msg) << CAF_ARG(mid));
  auto src = src_nid == this_node() ? system().registry().get(src_aid)
                                    : proxies().get_or_put(src_nid, src_aid);
  if (! dest) {
    auto rsn = exit_reason::remote_link_unreachable;
    CAF_LOG_INFO("cannot deliver message, destination not found");
    self->parent().notify<hook::invalid_message_received>(src_nid, src,
                                                          invalid_actor_id,
                                                          mid, msg);
    if (mid.valid() && src) {
      detail::sync_request_bouncer srb{rsn};
      srb(src, mid);
    }
    return;
  }
  self->parent().notify<hook::message_received>(src_nid, src, dest, mid, msg);
  dest->enqueue(make_mailbox_element(std::move(src), mid, std::move(stages),
                                      std::move(msg)),
                nullptr);
}

void basp_broker_state::learned_new_node(const node_id& nid) {
  CAF_LOG_TRACE(CAF_ARG(nid));
  if (spawn_servers.count(nid) > 0) {
    CAF_LOG_ERROR("learned_new_node called for known node " << CAF_ARG(nid));
    return;
  }
  auto tmp = system().spawn<hidden>([=](event_based_actor* tself) -> behavior {
    CAF_LOG_TRACE("");
    // terminate when receiving a down message
    tself->set_down_handler([=](down_msg& dm) {
      CAF_LOG_TRACE(CAF_ARG(dm));
      tself->quit(std::move(dm.reason));
    });
    // skip messages until we receive the initial ok_atom
    tself->set_default_handler(skip);
    return {
      [=](ok_atom, const std::string& /* key == "info" */,
          const strong_actor_ptr& config_serv, const std::string& /* name */) {
        CAF_LOG_TRACE(CAF_ARG(config_serv));
        // drop unexpected messages from this point on
        tself->set_default_handler(print_and_drop);
        if (! config_serv)
          return;
        tself->monitor(config_serv);
        tself->become(
          [=](spawn_atom, std::string& type, message& args)
          -> delegated<ok_atom, strong_actor_ptr, std::set<std::string>> {
            CAF_LOG_TRACE(CAF_ARG(type) << CAF_ARG(args));
            tself->delegate(actor_cast<actor>(std::move(config_serv)),
                                 get_atom::value, std::move(type),
                                 std::move(args));
            return {};
          }
        );
      },
      after(std::chrono::minutes(5)) >> [=] {
        CAF_LOG_INFO("no spawn server found:" << CAF_ARG(nid));
        tself->quit();
      }
    };
  });
  spawn_servers.emplace(nid, tmp);
  using namespace detail;
  system().registry().put(tmp.id(), actor_cast<strong_actor_ptr>(tmp));
  auto writer = make_callback([](serializer& sink) {
    auto name_atm = atom("SpawnServ");
    std::vector<actor_id> stages;
    auto msg = make_message(sys_atom::value, get_atom::value, "info");
    sink << name_atm << stages << msg;
  });
  auto path = instance.tbl().lookup(nid);
  if (! path) {
    CAF_LOG_ERROR("learned_new_node called, but no route to nid");
    return;
  }
  // send message to SpawnServ of remote node
  basp::header hdr{basp::message_type::dispatch_message,
                   basp::header::named_receiver_flag,
                   0, 0, this_node(), nid, tmp.id(), invalid_actor_id};
  // writing std::numeric_limits<actor_id>::max() is a hack to get
  // this send-to-named-actor feature working with older CAF releases
  instance.write(self->context(), path->wr_buf, hdr, &writer);
  /*
                 basp::message_type::dispatch_message,
                 nullptr, static_cast<uint64_t>(atom("SpawnServ")),
                 this_node(), nid,
                 tmp.id(), std::numeric_limits<actor_id>::max(),
                 &writer);
  */
  instance.flush(*path);
}

void basp_broker_state::learned_new_node_directly(const node_id& nid,
                                                  bool was_indirectly_before) {
  CAF_ASSERT(this_context != nullptr);
  CAF_LOG_TRACE(CAF_ARG(nid));
  if (! was_indirectly_before)
    learned_new_node(nid);
}

void basp_broker_state::learned_new_node_indirectly(const node_id& nid) {
  CAF_ASSERT(this_context != nullptr);
  CAF_LOG_TRACE(CAF_ARG(nid));
  learned_new_node(nid);
  if (! enable_automatic_connections)
    return;
  // this member function gets only called once, after adding a new
  // indirect connection to the routing table; hence, spawning
  // our helper here exactly once and there is no need to track
  // in-flight connection requests
  auto connection_helper = [=](event_based_actor* helper, actor s) -> behavior {
    CAF_LOG_TRACE(CAF_ARG(s));
    helper->monitor(s);
    helper->set_down_handler([=](down_msg& dm) {
      CAF_LOG_TRACE(CAF_ARG(dm));
      helper->quit(std::move(dm.reason));
    });
    return {
      // this config is send from the remote `ConfigServ`
      [=](ok_atom, const std::string&, message& msg) {
        CAF_LOG_TRACE(CAF_ARG(msg));
        CAF_LOG_DEBUG("received requested config:" << CAF_ARG(msg));
        // whatever happens, we are done afterwards
        helper->quit();
        msg.apply({
          [&](uint16_t port, network::address_listing& addresses) {
            auto& mx = system().middleman().backend();
            for (auto& kvp : addresses)
              if (kvp.first != network::protocol::ethernet)
                for (auto& addr : kvp.second) {
                auto hdl = mx.new_tcp_scribe(addr, port);
                if (hdl) {
                  // gotcha! send scribe to our BASP broker
                  // to initiate handshake etc.
                  CAF_LOG_INFO("connected directly:" << CAF_ARG(addr));
                  helper->send(s, connect_atom::value, *hdl, port);
                  return;
                }
                // else: simply try next address
              }
            CAF_LOG_INFO("could not connect to node directly:" << CAF_ARG(nid));
          }
        });
      },
      after(std::chrono::minutes(10)) >> [=] {
        CAF_LOG_TRACE(CAF_ARG(""));
        // nothing heard in about 10 minutes... just a call it a day, then
        CAF_LOG_INFO("aborted direct connection attempt after 10min:"
                      << CAF_ARG(nid));
        helper->quit(exit_reason::user_shutdown);
      }
    };
  };
  auto path = instance.tbl().lookup(nid);
  if (! path) {
    CAF_LOG_ERROR("learned_new_node_indirectly called, but no route to nid");
    return;
  }
  if (path->next_hop == nid) {
    CAF_LOG_ERROR("learned_new_node_indirectly called with direct connection");
    return;
  }
  using namespace detail;
  auto tmp = system().spawn<detached + hidden>(connection_helper, self);
  system().registry().put(tmp.id(), actor_cast<strong_actor_ptr>(tmp));
  auto writer = make_callback([](serializer& sink) {
    auto name_atm = atom("ConfigServ");
    std::vector<actor_id> stages;
    auto msg = make_message(get_atom::value, "basp.default-connectivity");
    sink << name_atm << stages << msg;
  });
  basp::header hdr{basp::message_type::dispatch_message,
                   basp::header::named_receiver_flag,
                   0, 0, this_node(), nid, tmp.id(), invalid_actor_id};
  instance.write(self->context(), path->wr_buf, hdr, &writer);
  instance.flush(*path);
}

void basp_broker_state::set_context(connection_handle hdl) {
  CAF_LOG_TRACE(CAF_ARG(hdl));
  auto i = ctx.find(hdl);
  if (i == ctx.end()) {
    CAF_LOG_INFO("create new BASP context:" << CAF_ARG(hdl));
    i = ctx.emplace(hdl,
                    connection_context{
                      basp::await_header,
                      basp::header{basp::message_type::server_handshake, 0,
                                   0, 0, invalid_node_id, invalid_node_id,
                                   invalid_actor_id, invalid_actor_id},
                      hdl,
                      invalid_node_id,
                      0,
                      none}).first;
  }
  this_context = &i->second;
}

/******************************************************************************
 *                                basp_broker                                 *
 ******************************************************************************/

basp_broker::basp_broker(actor_config& cfg)
    : stateful_actor<basp_broker_state, broker>(cfg) {
  // nop
}

behavior basp_broker::make_behavior() {
  CAF_LOG_TRACE(CAF_ARG(system().node()));
  if (system().config().middleman_enable_automatic_connections) {
    CAF_LOG_INFO("enable automatic connections");
    // open a random port and store a record for our peers how to
    // connect to this broker directly in the configuration server
    //auto port =
    auto res = add_tcp_doorman(uint16_t{0});
    if (res) {
      auto port = res->second;
      auto addrs = network::interfaces::list_addresses(false);
      auto config_server = system().registry().get(atom("ConfigServ"));
      send(actor_cast<actor>(config_server), put_atom::value,
           "basp.default-connectivity",
           make_message(port, std::move(addrs)));
      state.enable_automatic_connections = true;
    }
  }
  auto heartbeat_interval = system().config().middleman_heartbeat_interval;
  if (heartbeat_interval > 0) {
    CAF_LOG_INFO("enable heartbeat" << CAF_ARG(heartbeat_interval));
    send(this, tick_atom::value, heartbeat_interval);
  }
  return {
    // received from underlying broker implementation
    [=](new_data_msg& msg) {
      CAF_LOG_TRACE(CAF_ARG(msg.handle));
      state.set_context(msg.handle);
      auto& ctx = *state.this_context;
      auto next = state.instance.handle(context(), msg, ctx.hdr,
                                        ctx.cstate == basp::await_payload);
      if (next == basp::close_connection) {
        if (ctx.callback) {
          CAF_LOG_WARNING("failed to handshake with remote node"
                          << CAF_ARG(msg.handle));
          ctx.callback->deliver(ok_atom::value,
                                node_id{invalid_node_id},
                                strong_actor_ptr{nullptr},
                                std::set<std::string>{});
        }
        close(msg.handle);
        state.ctx.erase(msg.handle);
      } else if (next != ctx.cstate) {
        auto rd_size = next == basp::await_payload
                       ? ctx.hdr.payload_len
                       : basp::header_size;
        configure_read(msg.handle, receive_policy::exactly(rd_size));
        ctx.cstate = next;
      }
    },
    // received from proxy instances
    [=](forward_atom, strong_actor_ptr& src,
        const std::vector<strong_actor_ptr>& fwd_stack,
        strong_actor_ptr& dest, message_id mid, const message& msg) {
      CAF_LOG_TRACE(CAF_ARG(src) << CAF_ARG(dest)
                    << CAF_ARG(mid) << CAF_ARG(msg));
      if (! dest || system().node() == dest->node()) {
        CAF_LOG_WARNING("cannot forward to invalid or local actor:"
                        << CAF_ARG(dest));
        return;
      }
      if (src && system().node() == src->node())
        system().registry().put(src->id(), src);
      if (! state.instance.dispatch(context(), src, fwd_stack,
                                    dest, mid, msg)
          && mid.is_request()) {
        detail::sync_request_bouncer srb{exit_reason::remote_link_unreachable};
        srb(src, mid);
      }
    },
    // received from some system calls like whereis
    [=](forward_atom, strong_actor_ptr& src,
        const node_id& dest_node, atom_value dest_name,
        const message& msg) -> result<void> {
      CAF_LOG_TRACE(CAF_ARG(src)
                    << ", " << CAF_ARG(dest_node)
                    << ", " << CAF_ARG(dest_name)
                    << ", " << CAF_ARG(msg));
      if (! src)
        return sec::cannot_forward_to_invalid_actor;
      auto path = this->state.instance.tbl().lookup(dest_node);
      if (! path) {
        CAF_LOG_ERROR("no route to receiving node");
        return sec::no_route_to_receiving_node;
      }
      if (system().node() == src->node())
        system().registry().put(src->id(), src);
      auto writer = make_callback([&](serializer& sink) {
        std::vector<actor_addr> stages;
        sink << dest_name << stages << msg;
      });
      basp::header hdr{basp::message_type::dispatch_message,
                       basp::header::named_receiver_flag,
                       0, 0, state.this_node(), dest_node,
                       src->id(), invalid_actor_id};
      state.instance.write(context(), path->wr_buf, hdr, &writer);
      state.instance.flush(*path);
      return unit;
    },
    // received from underlying broker implementation
    [=](const new_connection_msg& msg) {
      CAF_LOG_TRACE(CAF_ARG(msg.handle));
      auto& bi = state.instance;
      bi.write_server_handshake(context(), wr_buf(msg.handle),
                                local_port(msg.source));
      flush(msg.handle);
      configure_read(msg.handle, receive_policy::exactly(basp::header_size));
    },
    // received from underlying broker implementation
    [=](const connection_closed_msg& msg) {
      CAF_LOG_TRACE(CAF_ARG(msg.handle));
      // TODO: currently we assume a node has gone offline once we lose
      //       a connection, we also could try to reach this node via other
      //       hops to be resilient to (rare) network failures or if a
      //       node is reachable via several interfaces and only one fails
      auto nid = state.instance.tbl().lookup_direct(msg.handle);
      // tell BASP instance we've lost connection
      state.instance.handle_node_shutdown(nid);
      CAF_ASSERT(nid == invalid_node_id
                 || ! state.instance.tbl().reachable(nid));
    },
    // received from underlying broker implementation
    [=](const acceptor_closed_msg& msg) {
      CAF_LOG_TRACE("");
      auto port = local_port(msg.handle);
      state.instance.remove_published_actor(port);
    },
    // received from middleman actor
    [=](publish_atom, accept_handle hdl, uint16_t port,
        const strong_actor_ptr& whom, std::set<std::string>& sigs) {
      CAF_LOG_TRACE(CAF_ARG(hdl.id()) << CAF_ARG(whom) << CAF_ARG(port));
      if (hdl.invalid()) {
        CAF_LOG_WARNING("invalid handle");
        return;
      }
      try {
        assign_tcp_doorman(hdl);
      }
      catch (...) {
        CAF_LOG_DEBUG("failed to assign doorman from handle");
        return;
      }
      if (whom)
        system().registry().put(whom->id(), whom);
      state.instance.add_published_actor(port, whom, std::move(sigs));
    },
    // received from middleman actor (delegated)
    [=](connect_atom, connection_handle hdl, uint16_t port) {
      CAF_LOG_TRACE(CAF_ARG(hdl.id()));
      auto rp = make_response_promise();
      try {
        assign_tcp_scribe(hdl);
      }
      catch (std::exception& e) {
        CAF_LOG_DEBUG("failed to assign scribe from handle: " << e.what());
        CAF_IGNORE_UNUSED(e);
        rp.deliver(sec::failed_to_assign_scribe_from_handle);
        return;
      }
      auto& ctx = state.ctx[hdl];
      ctx.hdl = hdl;
      ctx.remote_port = port;
      ctx.cstate = basp::await_header;
      ctx.callback = rp;
      // await server handshake
      configure_read(hdl, receive_policy::exactly(basp::header_size));
    },
    [=](delete_atom, const node_id& nid, actor_id aid) {
      CAF_LOG_TRACE(CAF_ARG(nid) << ", " << CAF_ARG(aid));
      state.proxies().erase(nid, aid);
    },
    [=](unpublish_atom, const actor_addr& whom, uint16_t port) -> result<void> {
      CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(port));
      auto cb = make_callback([&](const strong_actor_ptr&, uint16_t x) {
        try { close(hdl_by_port(x)); }
        catch (std::exception&) { }
      });
      if (state.instance.remove_published_actor(whom, port, &cb) == 0)
        return sec::no_actor_published_at_port;
      return unit;
    },
    [=](close_atom, uint16_t port) -> result<void> {
      if (port == 0)
        return sec::cannot_close_invalid_port;
      // it is well-defined behavior to not have an actor published here,
      // hence the result can be ignored safely
      state.instance.remove_published_actor(port, nullptr);
      try {
        close(hdl_by_port(port));
      }
      catch (std::exception&) {
        return sec::cannot_close_invalid_port;
      }
      return unit;
    },
    [=](spawn_atom, const node_id& nid, std::string& type, message& xs)
    -> delegated<ok_atom, strong_actor_ptr, std::set<std::string>> {
      CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(type) << CAF_ARG(xs));
      auto i = state.spawn_servers.find(nid);
      if (i == state.spawn_servers.end()) {
        auto rp = make_response_promise();
        rp.deliver(sec::no_route_to_receiving_node);
      } else {
        delegate(i->second, spawn_atom::value, std::move(type), std::move(xs));
      }
      return {};
    },
    [=](get_atom, const node_id& x)
    -> std::tuple<node_id, std::string, uint16_t> {
      std::string addr;
      uint16_t port = 0;
      auto hdl = state.instance.tbl().lookup_direct(x);
      if (hdl != invalid_connection_handle) {
        addr = remote_addr(hdl);
        port = remote_port(hdl);
      }
      return std::make_tuple(x, std::move(addr), port);
    },
    [=](tick_atom, size_t interval) {
      state.instance.handle_heartbeat(context());
      delayed_send(this, std::chrono::milliseconds{interval},
                   tick_atom::value, interval);
    }
  };
}

resumable::resume_result basp_broker::resume(execution_unit* ctx, size_t mt) {
  ctx->proxy_registry_ptr(&state.instance.proxies());
  auto guard = detail::make_scope_guard([=] {
    ctx->proxy_registry_ptr(nullptr);
  });
  return super::resume(ctx, mt);
}

proxy_registry* basp_broker::proxy_registry_ptr() {
  return &state.instance.proxies();
}

} // namespace io
} // namespace caf
