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

#include "caf/send.hpp"
#include "caf/exception.hpp"
#include "caf/make_counted.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/forwarding_actor_proxy.hpp"

#include "caf/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/io/basp.hpp"
#include "caf/io/middleman.hpp"

#include "caf/io/network/interfaces.hpp"

namespace caf {
namespace io {

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

actor_proxy_ptr basp_broker_state::make_proxy(node_id nid, actor_id aid) {
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
  intrusive_ptr<basp_broker> ptr = static_cast<basp_broker*>(self);
  auto mm = &system().middleman();
  auto res = make_counted<forwarding_actor_proxy>(aid, nid, ptr);
  res->attach_functor([=](uint32_t rsn) {
    mm->backend().post([=] {
      // using res->id() instead of aid keeps this actor instance alive
      // until the original instance terminates, thus preventing subtle
      // bugs with attachables
      if (ptr->exit_reason() == exit_reason::not_exited)
        ptr->state.proxies().erase(nid, res->id(), rsn);
    });
  });
  CAF_LOG_INFO("successfully created proxy instance, "
               "write announce_proxy_instance:"
               << CAF_ARG(nid) << CAF_ARG(aid));
  // tell remote side we are monitoring this actor now
  instance.write(self->context(),
                 self->wr_buf(this_context->hdl),
                 basp::message_type::announce_proxy_instance, nullptr, 0,
                 this_node(), nid,
                 invalid_actor_id, aid);
  instance.tbl().flush(*path);
  mm->notify<hook::new_remote_actor>(res->address());
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
  if (aid == invalid_actor_id) {
    // can occur when connecting to the default port of a node
    cb->deliver(make_message(ok_atom::value,
                             nid,
                             actor_addr{invalid_actor_addr},
                             std::move(sigs)));
    return;
  }
  actor_addr addr;
  if (nid == this_node()) {
    // connected to self
    addr = system().registry().get(aid).first;
    CAF_LOG_INFO_IF(! addr, "actor not found:" << CAF_ARG(aid));
  } else {
    auto ptr = namespace_.get_or_put(nid, aid);
    if (ptr)
      addr = ptr->address();
    CAF_LOG_ERROR_IF(! ptr, "creating actor in finalize_handshake failed");
  }
  if (addr.node() != system().node())
    known_remotes.emplace(nid, std::make_pair(this_context->remote_port, addr));
  cb->deliver(make_message(ok_atom::value, nid, addr, std::move(sigs)));
  this_context->callback = none;
}

void basp_broker_state::purge_state(const node_id& nid) {
  CAF_LOG_TRACE(CAF_ARG(nid));
  auto hdl = instance.tbl().lookup_direct(nid);
  if (hdl == invalid_connection_handle)
    return;
  proxies().erase(nid);
  ctx.erase(hdl);
  known_remotes.erase(nid);
}

void basp_broker_state::proxy_announced(const node_id& nid, actor_id aid) {
  CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(aid));
  // source node has created a proxy for one of our actors
  auto entry = system().registry().get(aid);
  auto send_kill_proxy_instance = [=](uint32_t rsn) {
    auto path = instance.tbl().lookup(nid);
    if (! path) {
      CAF_LOG_INFO("cannot send exit message for proxy, no route to host:"
                   << CAF_ARG(nid));
      return;
    }
    instance.write_kill_proxy_instance(self->context(), path->wr_buf,
                                       nid, aid, rsn);
    instance.tbl().flush(*path);
  };
  if (entry.second != exit_reason::not_exited) {
    CAF_LOG_DEBUG("kill proxy immediately");
    // kill immediately if actor has already terminated
    send_kill_proxy_instance(entry.second);
  } else {
    intrusive_ptr<broker> bptr = self; // keep broker alive ...
    auto mm = &system().middleman();
    entry.first->attach_functor([=](uint32_t reason) {
      mm->backend().dispatch([=] {
        CAF_LOG_TRACE(CAF_ARG(reason));
        // ... to make sure this is safe
        if (bptr == mm->named_broker<basp_broker>(atom("BASP"))
            && bptr->exit_reason() == exit_reason::not_exited)
          send_kill_proxy_instance(reason);
      });
    });
  }
}

void basp_broker_state::kill_proxy(const node_id& nid, actor_id aid,
                                   uint32_t rsn) {
  CAF_LOG_TRACE(CAF_ARG(nid) << CAF_ARG(aid) << CAF_ARG(rsn));
  proxies().erase(nid, aid, rsn);
}

void basp_broker_state::deliver(const node_id& source_node,
                                actor_id source_actor,
                                const node_id& dest_node,
                                actor_id dest_actor,
                                message& msg,
                                message_id mid) {
  CAF_LOG_TRACE(CAF_ARG(source_node)
                << CAF_ARG(source_actor) << CAF_ARG(dest_node)
                << CAF_ARG(dest_actor) << CAF_ARG(msg) << CAF_ARG(mid));
  actor_addr src;
  if (source_node == this_node()) {
    auto src_ptr = system().registry().get(source_actor).first;
    if (src_ptr)
      src = src_ptr->address();
  } else {
    auto ptr = proxies().get_or_put(source_node, source_actor);
    if (ptr)
      src = ptr->address();
  }
  actor_addr dest;
  uint32_t rsn = exit_reason::remote_link_unreachable;
  if (dest_node != this_node()) {
    auto ptr = proxies().get_or_put(dest_node, dest_actor);
    if (ptr)
      dest = ptr->address();
  } else {
    if (dest_actor == std::numeric_limits<actor_id>::max()) {
      // this hack allows CAF to talk to older CAF versions; CAF <= 0.14 will
      // discard this message silently as the receiver is not found, while
      // CAF >= 0.14.1 will use the operation data to identify named actors
      auto dest_name = static_cast<atom_value>(mid.integer_value());
      CAF_LOG_DEBUG(CAF_ARG(dest_name));
      mid = message_id::make(); // override this since the message is async
      dest = system().registry().get(dest_name).address();
    } else {
      std::tie(dest, rsn) = system().registry().get(dest_actor);
    }
  }
  if (! dest) {
    CAF_LOG_INFO("cannot deliver message, destination not found");
    self->parent().notify<hook::invalid_message_received>(source_node, src,
                                                          dest_actor, mid, msg);
    if (mid.valid() && src != invalid_actor_addr) {
      detail::sync_request_bouncer srb{rsn};
      srb(src, mid);
    }
    return;
  }
  self->parent().notify<hook::message_received>(source_node, src,
                                                dest->address(), mid, msg);
  dest->enqueue(src, mid, std::move(msg), nullptr);
}

void basp_broker_state::learned_new_node(const node_id& nid) {
  CAF_LOG_TRACE(CAF_ARG(nid));
  auto& tmp = spawn_servers[nid];
  tmp = system().spawn<hidden>([=](event_based_actor* this_actor) -> behavior {
    CAF_LOG_TRACE("");
    return {
      [=](ok_atom, const std::string& /* key == "info" */,
          const actor_addr& config_serv_addr, const std::string& /* name */) {
        CAF_LOG_TRACE(CAF_ARG(config_serv_addr));
        auto config_serv = actor_cast<actor>(config_serv_addr);
        this_actor->monitor(config_serv);
        this_actor->become(
          [=](spawn_atom, std::string& type, message& args)
          -> delegated<either<ok_atom, actor_addr, std::set<std::string>>
                       ::or_else<error_atom, std::string>> {
            CAF_LOG_TRACE(CAF_ARG(type) << CAF_ARG(args));
            this_actor->delegate(config_serv, get_atom::value,
                                 std::move(type), std::move(args));
            return {};
          },
          [=](const down_msg& dm) {
            CAF_LOG_TRACE(CAF_ARG(dm));
            this_actor->quit(dm.reason);
          },
          others >> [=] {
            CAF_LOG_ERROR("spawn server has received unexpected message:"
                           << CAF_ARG(this_actor->current_message()));
          }
        );
      },
      after(std::chrono::minutes(5)) >> [=] {
        CAF_LOG_INFO("no spawn server found:" << CAF_ARG(nid));
        this_actor->quit();
      }
    };
  });
  using namespace detail;
  system().registry().put(tmp.id(), tmp.address());
  auto writer = make_callback([](serializer& sink) {
    auto msg = make_message(sys_atom::value, get_atom::value, "info");
    sink << msg;
  });
  auto path = instance.tbl().lookup(nid);
  if (! path) {
    CAF_LOG_ERROR("learned_new_node called, but no route to nid");
    return;
  }
  // writing std::numeric_limits<actor_id>::max() is a hack to get
  // this send-to-named-actor feature working with older CAF releases
  instance.write(self->context(),
                 path->wr_buf,
                 basp::message_type::dispatch_message,
                 nullptr, static_cast<uint64_t>(atom("SpawnServ")),
                 this_node(), nid,
                 tmp.id(), std::numeric_limits<actor_id>::max(),
                 &writer);
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
  actor bb = self; // a handle for our helper back to this BASP broker
  // this member function gets only called once, after adding a new
  // indirect connection to the routing table; hence, spawning
  // our helper here exactly once and there is no need to track
  // in-flight connection requests
  auto connection_helper = [=](event_based_actor* helper, actor s) -> behavior {
    CAF_LOG_TRACE(CAF_ARG(s));
    helper->monitor(s);
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
                  try {
                    auto hdl = mx.new_tcp_scribe(addr, port);
                    // gotcha! send scribe to our BASP broker
                    // to initiate handshake etc.
                    CAF_LOG_INFO("connected directly:" << CAF_ARG(addr));
                    helper->send(bb, connect_atom::value, hdl, port);
                    return;
                  }
                  catch (...) {
                    // simply try next address
                  }
                }
            CAF_LOG_INFO("could not connect to node directly:" << CAF_ARG(nid));
          }
        });
      },
      [=](const down_msg& dm) {
        CAF_LOG_TRACE(CAF_ARG(dm));
        helper->quit(dm.reason);
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
  system().registry().put(tmp.id(), tmp.address());
  auto writer = make_callback([](serializer& sink) {
    auto msg = make_message(get_atom::value, "basp.default-connectivity");
    sink << msg;
  });
  // writing std::numeric_limits<actor_id>::max() is a hack to get
  // this send-to-named-actor feature working with older CAF releases
  instance.write(self->context(),
                 path->wr_buf,
                 basp::message_type::dispatch_message,
                 nullptr, static_cast<uint64_t>(atom("ConfigServ")),
                 this_node(), nid,
                 tmp.id(), std::numeric_limits<actor_id>::max(),
                 &writer);
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
                      basp::header{basp::message_type::server_handshake, 0, 0,
                                   invalid_node_id, invalid_node_id,
                                   invalid_actor_id, invalid_actor_id},
                      hdl,
                      invalid_node_id,
                      0,
                      none}).first;
  }
  this_context = &i->second;
}

bool basp_broker_state::erase_context(connection_handle hdl) {
  CAF_LOG_TRACE(CAF_ARG(hdl));
  auto i = ctx.find(hdl);
  if (i == ctx.end())
    return false;
  auto& ref = i->second;
  if (ref.callback) {
    CAF_LOG_DEBUG("connection closed during handshake");
    ref.callback->deliver(make_message(error_atom::value,
                                       "disconnect during handshake"));
  }
  ctx.erase(i);
  return true;
}

/******************************************************************************
 *                                basp_broker                                 *
 ******************************************************************************/

basp_broker::basp_broker(actor_config& cfg)
    : stateful_actor<basp_broker_state, broker>(cfg) {
  // nop
}

behavior basp_broker::make_behavior() {
  // TODO: query this config from middleman directly
  // ask the configuration server whether we should open a default port
  auto config_server = system().registry().get(atom("ConfigServ"));
  send(config_server, get_atom::value, "middleman.enable-automatic-connections");
  return {
    // received from underlying broker implementation
    [=](new_data_msg& msg) {
      CAF_LOG_TRACE(CAF_ARG(msg.handle));
      state.set_context(msg.handle);
      auto& ctx = *state.this_context;
      auto next = state.instance.handle(context(), msg, ctx.hdr,
                                        ctx.cstate == basp::await_payload);
      if (next == basp::close_connection) {
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
    [=](forward_atom, const actor_addr& sender, const actor_addr& receiver,
        message_id mid, const message& msg) {
      CAF_LOG_TRACE(CAF_ARG(sender) << CAF_ARG(receiver)
                    << CAF_ARG(mid) << CAF_ARG(msg));
      if (! receiver || system().node() == receiver.node()) {
        CAF_LOG_WARNING("cannot forward to invalid or local actor:"
                        << CAF_ARG(receiver));
        return;
      }
      if (sender && system().node() == sender.node())
        system().registry().put(sender->id(), sender);
      if (! state.instance.dispatch(context(), sender, receiver, mid, msg)
          && mid.is_request()) {
        detail::sync_request_bouncer srb{exit_reason::remote_link_unreachable};
        srb(sender, mid);
      }
    },
    // received from some system calls like whereis
    [=](forward_atom, const actor_addr& sender,
        const node_id& receiving_node, atom_value receiver_name,
        const message& msg) -> maybe<message> {
      CAF_LOG_TRACE(CAF_ARG(sender)
                    << ", " << CAF_ARG(receiving_node)
                    << ", " << CAF_ARG(receiver_name)
                    << ", " << CAF_ARG(msg));
      if (sender == invalid_actor_addr)
        return make_message(error_atom::value, "sender == invalid_actor");
      if (system().node() == sender.node())
        system().registry().put(sender->id(), sender);
      auto writer = make_callback([&](serializer& sink) {
        sink << msg;
      });
      auto path = this->state.instance.tbl().lookup(receiving_node);
      if (! path) {
        CAF_LOG_ERROR("no route to receiving node");
        return make_message(error_atom::value, "no route to receiving node");
      }
      // writing std::numeric_limits<actor_id>::max() is a hack to get
      // this send-to-named-actor feature working with older CAF releases
      this->state.instance.write(context(),
                                 path->wr_buf,
                                 basp::message_type::dispatch_message,
                                 nullptr, static_cast<uint64_t>(receiver_name),
                                 state.this_node(), receiving_node,
                                 sender.id(),
                                 std::numeric_limits<actor_id>::max(),
                                 &writer);
      state.instance.flush(*path);
      return none;
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
      if (! state.erase_context(msg.handle))
        return;
      // TODO: currently we assume a node has gone offline once we lose
      //       a connection, we also could try to reach this node via other
      //       hops to be resilient to (rare) network failures or if a
      //       node is reachable via several interfaces and only one fails
      auto nid = state.instance.tbl().lookup_direct(msg.handle);
      if (nid != invalid_node_id) {
        // tell BASP instance we've lost connection
        state.instance.handle_node_shutdown(nid);
        // remove all proxies
        state.proxies().erase(nid);
      }
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
    [=](publish_atom, accept_handle hdl, uint16_t port, const actor_addr& whom,
        std::set<std::string>& sigs) {
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
      if (whom != invalid_actor_addr) {
        system().registry().put(whom->id(), whom);
      }
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
        std::string err = "failed to assign scribe from handle: ";
        err += e.what();
        rp.deliver(make_message(error_atom::value, std::move(err)));
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
    [=](unpublish_atom, const actor_addr& whom, uint16_t port) -> message {
      CAF_LOG_TRACE(CAF_ARG(whom) << ", " << CAF_ARG(port));
      if (whom == invalid_actor_addr)
        return make_message(error_atom::value, "whom == invalid_actor_addr");
      auto cb = make_callback([&](const actor_addr&, uint16_t x) {
        try { close(hdl_by_port(x)); }
        catch (std::exception&) { }
      });
      return state.instance.remove_published_actor(whom, port, &cb) == 0
               ? make_message(error_atom::value, "no mapping found")
               : make_message(ok_atom::value);
    },
    [=](close_atom, uint16_t port) -> message {
      if (port == 0)
        return make_message(error_atom::value, "port == 0");
      // it is well-defined behavior to not have an actor published here,
      // hence the result can be ignored safely
      state.instance.remove_published_actor(port, nullptr);
      try {
        close(hdl_by_port(port));
        return make_message(ok_atom::value);
      }
      catch (std::exception&) {
        return make_message(error_atom::value,
          "no doorman for given port found");
      }
    },
    [=](spawn_atom, const node_id& nid, std::string& type, message& args)
    -> delegated<either<ok_atom, actor_addr, std::set<std::string>>
                 ::or_else<error_atom, std::string>> {
      auto err = [=](std::string errmsg) {
        auto rp = make_response_promise();
        rp.deliver(error_atom::value, std::move(errmsg));
      };
      auto i = state.spawn_servers.find(nid);
      if (i == state.spawn_servers.end()) {
          err("no connection to requested node");
          return {};
      }
      delegate(i->second, spawn_atom::value, std::move(type), std::move(args));
      return {};
    },
    [=](ok_atom, const std::string& key, message& value) {
      if (key == "middleman.enable-automatic-connections") {
        value.apply([&](bool enabled) {
          if (! enabled)
            return;
          CAF_LOG_INFO("enable automatic connection");
          // open a random port and store a record for others
          // how to connect to this port in the configuration server
          auto port = add_tcp_doorman(uint16_t{0});
          auto addrs = network::interfaces::list_addresses(false);
          send(config_server, put_atom::value, "basp.default-connectivity",
               make_message(port.second, std::move(addrs)));
          state.enable_automatic_connections = true;
        });
      }
    },
    // catch-all error handler
    others >> [=] {
      CAF_LOG_ERROR("unexpected message:" << CAF_ARG(current_message()));
    }};
}

resumable::resume_result basp_broker::resume(execution_unit* ctx, size_t mt) {
  ctx->proxy_registry_ptr(&state.instance.proxies());
  auto guard = detail::make_scope_guard([=] {
    ctx->proxy_registry_ptr(nullptr);
  });
  return super::resume(ctx, mt);
}

void basp_broker::exec_single_event(execution_unit* ctx,
                                    mailbox_element_ptr& ptr) {
  CAF_ASSERT(ctx != nullptr);
  ctx->proxy_registry_ptr(&state.instance.proxies());
  auto guard = detail::make_scope_guard([=] {
    ctx->proxy_registry_ptr(nullptr);
  });
  super::exec_single_event(ctx, ptr);
}

} // namespace io
} // namespace caf
