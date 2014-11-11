/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include "caf/exception.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/make_counted.hpp"
#include "caf/detail/actor_registry.hpp"

#include "caf/io/basp.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/unpublish.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/remote_actor_proxy.hpp"

using std::string;

namespace caf {
namespace io {

using detail::singletons;
using detail::make_counted;

basp_broker::basp_broker() : m_namespace(*this) {
  m_meta_msg = uniform_typeid<message>();
  m_meta_id_type = uniform_typeid<node_id>();
  CAF_LOG_DEBUG("BASP broker started: " << to_string(node()));
}

behavior basp_broker::make_behavior() {
  return {
    // received from underlying broker implementation
    [=](new_data_msg& msg) {
      CAF_LOGM_TRACE("make_behavior$new_data_msg",
                     "handle = " << msg.handle.id());
      CAF_REQUIRE(m_ctx.count(msg.handle) > 0);
      new_data(m_ctx[msg.handle], msg.buf);
    },
    // received from underlying broker implementation
    [=](const new_connection_msg& msg) {
      CAF_LOGM_TRACE("make_behavior$new_connection_msg",
                     "handle = " << msg.handle.id());
      CAF_REQUIRE(m_ctx.count(msg.handle) == 0);
      auto& ctx = m_ctx[msg.handle];
      ctx.hdl = msg.handle;
      ctx.handshake_data = nullptr;
      ctx.state = await_client_handshake;
      init_handshake_as_sever(ctx, m_acceptors[msg.source].first->address());
    },
    // received from underlying broker implementation
    [=](const connection_closed_msg& msg) {
      CAF_LOGM_TRACE("make_behavior$connection_closed_msg",
                     CAF_MARG(msg.handle, id));
      auto j = m_ctx.find(msg.handle);
      if (j != m_ctx.end()) {
        auto hd = j->second.handshake_data;
        if (hd) {
          network_error err{"disconnect during handshake"};
          hd->result->set_exception(std::make_exception_ptr(err));
        }
        m_ctx.erase(j);
      }
      // purge handle from all routes
      std::vector<id_type> lost_connections;
      for (auto& kvp : m_routes) {
        auto& entry = kvp.second;
        if (entry.first.hdl == msg.handle) {
          CAF_LOG_DEBUG("lost direct connection to " << to_string(kvp.first));
          entry.first.hdl.set_invalid();
        }
        auto last = entry.second.end();
        auto i = std::lower_bound(entry.second.begin(), last, msg.handle,
                                  connection_info_less{});
        if (i != last && i->hdl == msg.handle) {
          entry.second.erase(i);
        }
        if (entry.first.invalid() && entry.second.empty()) {
          lost_connections.push_back(kvp.first);
        }
      }
      // remove routes that no longer have any path and kill all proxies
      for (auto& lc : lost_connections) {
        CAF_LOG_DEBUG("no more route to " << to_string(lc));
        m_routes.erase(lc);
        auto proxies = m_namespace.get_all(lc);
        m_namespace.erase(lc);
        for (auto& p : proxies) {
          p->kill_proxy(exit_reason::remote_link_unreachable);
        }
      }
    },
    // received from underlying broker implementation
    [=](const acceptor_closed_msg& msg) {
      CAF_LOGM_TRACE("make_behavior$acceptor_closed_msg", "");
      auto i = m_acceptors.find(msg.handle);
      if (i == m_acceptors.end()) {
        CAF_LOG_INFO("accept handle no longer in use");
        return;
      }
      if (m_open_ports.erase(i->second.second) == 0) {
        CAF_LOG_INFO("accept handle was not bound to a port");
      }
      m_acceptors.erase(i);
    },
    // received from proxy instances
    on(atom("_Dispatch"), arg_match) >> [=](const actor_addr& sender,
                                            const actor_addr& receiver,
                                            message_id mid,
                                            const message& msg) {
      CAF_LOGM_TRACE("make_behavior$_Dispatch", "");
      dispatch(sender, receiver, mid, msg);
    },
    on(atom("_DelProxy"), arg_match) >> [=](const id_type& nid, actor_id aid) {
      CAF_LOGM_TRACE("make_behavior$_DelProxy",
                   CAF_TSARG(nid) << ", "
                   << CAF_ARG(aid));
      erase_proxy(nid, aid);
    },
    others() >> [=] {
      CAF_LOG_INFO("received unexpected message: "
                   << to_string(last_dequeued()));
    }
  };
}

void basp_broker::new_data(connection_context& ctx, buffer_type& buf) {
  CAF_LOG_TRACE(CAF_TARG(ctx.state, static_cast<int>) << ", "
                CAF_MARG(ctx.hdl, id));
  m_current_context = &ctx;
  connection_state next_state;
  switch (ctx.state) {
    default: {
      binary_deserializer bd{buf.data(), buf.size(), &m_namespace};
      read(bd, ctx.hdr);
      if (!basp::valid(ctx.hdr)) {
        CAF_LOG_INFO("invalid broker message received");
        close(ctx.hdl);
        m_ctx.erase(ctx.hdl);
        return;
      }
      next_state = handle_basp_header(ctx);
      break;
    }
    case await_payload: {
      next_state = handle_basp_header(ctx, &buf);
      break;
    }
  }
  CAF_LOG_DEBUG("transition: " << ctx.state << " -> " << next_state);
  if (next_state == close_connection) {
    close(ctx.hdl);
    m_ctx.erase(ctx.hdl);
    return;
  }
  ctx.state = next_state;
  configure_read(ctx.hdl, receive_policy::exactly(next_state == await_payload
                                                    ? ctx.hdr.payload_len
                                                    : basp::header_size));
}

void basp_broker::dispatch(const basp::header& hdr, message&& msg) {
  CAF_LOG_TRACE("");
  // TODO: provide hook API to allow ActorShell to
  //       intercept/trace/log each message
  actor_addr src;
  if (hdr.source_node != invalid_node_id
      && hdr.source_actor != invalid_actor_id) {
    if (hdr.source_node != node()) {
      CAF_LOG_DEBUG("source is a proxy");
      src = m_namespace.get_or_put(hdr.source_node,
                                   hdr.source_actor)->address();
    } else {
      CAF_LOG_DEBUG("source is a local actor (so he claims)");
      auto ptr = singletons::get_actor_registry()->get(hdr.source_actor);
      if (ptr) {
        src = ptr->address();
      }
    }
  }
  CAF_LOG_DEBUG_IF(src == invalid_actor_addr, "src == invalid_actor_addr");
  auto dest = singletons::get_actor_registry()->get(hdr.dest_actor);
  CAF_REQUIRE(!dest || dest->node() == node());
  // intercept message used for link signaling
  if (dest && src == dest) {
    if (msg.size() == 2 && typeid(actor_addr) == *msg.type_at(1)) {
      actor_addr other;
      bool is_unlink = true;
      // extract arguments
      msg.apply({
        on(atom("_Link"), arg_match) >> [&](const actor_addr& addr) {
          is_unlink = false;
          other = addr;
        },
        on(atom("_Unlink"), arg_match) >> [&](const actor_addr& addr) {
          other = addr;
        }
      });
      // in both cases, we link a local actor to a proxy
      if (other != invalid_actor_addr) {
        auto iptr = actor_cast<intrusive_ptr<abstract_actor>>(other);
        auto ptr = dynamic_cast<actor_proxy*>(iptr.get());
        if (ptr) {
          if (is_unlink) {
            ptr->local_unlink_from(dest->address());
          } else {
            // it's either an unlink request or a new link
            ptr->local_link_to(dest->address());
          }
          // do not actually send this message as it's been already handled
          return;
        }
      }
    }
  }
  auto mid = message_id::from_integer_value(hdr.operation_data);
  if (!dest) {
    CAF_LOG_DEBUG("received a message for an invalid actor; "
                  "could not find an actor with ID "
                  << hdr.dest_actor);
    parent().notify<hook::invalid_message_received>(hdr.source_node, src,
                                                    hdr.dest_actor, mid, msg);
    return;
  }
  parent().notify<hook::message_received>(hdr.source_node, src,
                                          dest->address(), mid, msg);
  CAF_LOG_DEBUG("enqueue message from " << to_string(src) << " to "
                << to_string(dest->address()));
  dest->enqueue(src, mid, std::move(msg), nullptr);
}

void basp_broker::dispatch(const actor_addr& from, const actor_addr& to,
                           message_id mid, const message& msg) {
  CAF_LOG_TRACE(CAF_TARG(from, to_string)
                << ", " << CAF_MARG(mid, integer_value) << ", "
                << CAF_TARG(to, to_string) << ", " << CAF_TARG(msg, to_string));
  CAF_REQUIRE(to != nullptr);
  auto dest = to.node();
  auto route = get_route(dest);
  if (route.invalid()) {
    parent().notify<hook::message_sending_failed>(from, to, mid, msg);
    CAF_LOG_INFO("unable to dispatch message: no route to "
                 << to_string(dest) << ", message: " << to_string(msg));
    return;
  }
  auto& buf = wr_buf(route.hdl);
  // reserve space in the buffer to write the broker message later on
  auto wr_pos = buf.size();
  char placeholder[basp::header_size];
  buf.insert(buf.end(), std::begin(placeholder), std::end(placeholder));
  auto before = buf.size();
  { // write payload, lifetime scope of first serializer
    binary_serializer bs1{std::back_inserter(buf), &m_namespace};
    bs1.write(msg, m_meta_msg);
  }
  // write broker message to the reserved space
  binary_serializer bs2{buf.begin() + wr_pos, &m_namespace};
  if (from != invalid_actor_addr && from.node() == node()) {
    // register locally running actors to be able to deserialize them later
    auto reg = detail::singletons::get_actor_registry();
    reg->put(from.id(), actor_cast<abstract_actor_ptr>(from));
  }
  write(bs2,
        {from.node(),                                dest,
         from.id(),                                  to.id(),
         static_cast<uint32_t>(buf.size() - before), basp::dispatch_message,
         mid.integer_value()});
  flush(route.hdl);
  parent().notify<hook::message_sent>(from, route.node, to, mid, msg);
}

void basp_broker::read(binary_deserializer& bd, basp::header& msg) {
  bd.read(msg.source_node, m_meta_id_type)
    .read(msg.dest_node, m_meta_id_type)
    .read(msg.source_actor)
    .read(msg.dest_actor)
    .read(msg.payload_len)
    .read(msg.operation)
    .read(msg.operation_data);
}

void basp_broker::write(binary_serializer& bs, const basp::header& msg) {
  bs.write(msg.source_node, m_meta_id_type)
    .write(msg.dest_node, m_meta_id_type)
    .write(msg.source_actor)
    .write(msg.dest_actor)
    .write(msg.payload_len)
    .write(msg.operation)
    .write(msg.operation_data);
}

basp_broker::connection_state
basp_broker::handle_basp_header(connection_context& ctx,
                                const buffer_type* payload) {
  CAF_LOG_TRACE(CAF_TARG(ctx.state, static_cast<int>)
                << ", payload = "
                << (payload ? payload->size() : 0) << " bytes"
                << (payload ? "" : " (nullptr)"));
  auto& hdr = ctx.hdr;
  if (!payload && hdr.payload_len > 0) {
    // receive payload first
    CAF_LOG_DEBUG("await payload");
    return await_payload;
  }
  CAF_LOG_DEBUG("header => "<< CAF_TSARG(hdr.source_node)
                << ", " << CAF_TSARG(hdr.dest_node)
                << ", " << CAF_ARG(hdr.source_actor)
                << ", " << CAF_ARG(hdr.dest_actor)
                << ", " << CAF_ARG(hdr.payload_len)
                << ", " << CAF_ARG(hdr.operation)
                << ", " << CAF_ARG(hdr.operation_data));
  // forward message if not addressed to us; invalid dest_node implies
  // that msg is a server_handshake
  if (hdr.dest_node != invalid_node_id && hdr.dest_node != node()) {
    auto route = get_route(hdr.dest_node);
    if (route.invalid()) {
      CAF_LOG_INFO("cannot forward message: no route to node "
                   << to_string(hdr.dest_node));
      parent().notify<hook::message_forwarding_failed>(hdr.source_node,
                                                       hdr.dest_node, payload);
      return close_connection;
    }
    CAF_LOG_DEBUG("received message that is not addressed to us -> "
                  << "forward via " << to_string(route.node));
    auto& buf = wr_buf(route.hdl);
    binary_serializer bs{std::back_inserter(buf), &m_namespace};
    write(bs, hdr);
    if (payload) {
      buf.insert(buf.end(), payload->begin(), payload->end());
    }
    flush(route.hdl);
    parent().notify<hook::message_forwarded>(hdr.source_node,
                                             hdr.dest_node, payload);
    return await_header;
  }
  // handle a message that is addressed to us
  switch (hdr.operation) {
    default:
      // must not happen
      throw std::logic_error("invalid operation");
    case basp::dispatch_message: {
      CAF_REQUIRE(payload != nullptr);
      binary_deserializer bd{payload->data(), payload->size(), &m_namespace};
      message content;
      bd.read(content, m_meta_msg);
      dispatch(ctx.hdr, std::move(content));
      break;
    }
    case basp::announce_proxy_instance: {
      CAF_REQUIRE(payload == nullptr);
      // source node has created a proxy for one of our actors
      auto entry = singletons::get_actor_registry()->get_entry(hdr.dest_actor);
      auto nid = hdr.source_node;
      auto aid = hdr.dest_actor;
      if (entry.second != exit_reason::not_exited) {
        send_kill_proxy_instance(nid, aid, entry.second);
      } else {
        auto mm = middleman::instance();
        entry.first->attach_functor([=](uint32_t reason) {
          mm->backend().dispatch([=] {
            CAF_LOGM_TRACE("handle_basp_header$proxy_functor",
                         CAF_ARG(reason));
            auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
            bro->send_kill_proxy_instance(nid, aid, reason);
          });
        });
      }
      break;
    }
    case basp::kill_proxy_instance: {
      CAF_REQUIRE(payload == nullptr);
      // we have a proxy to an actor that has been terminated
      auto ptr = m_namespace.get(hdr.source_node, hdr.source_actor);
      if (ptr) {
        m_namespace.erase(ptr->node(), ptr->id());
        ptr->kill_proxy(static_cast<uint32_t>(hdr.operation_data));
      } else {
        CAF_LOG_DEBUG("received kill proxy twice");
      }
      break;
    }
    case basp::client_handshake: {
      CAF_REQUIRE(payload == nullptr);
      if (ctx.remote_id != invalid_node_id) {
        CAF_LOG_INFO("received unexpected client handshake");
        return close_connection;
      }
      ctx.remote_id = hdr.source_node;
      if (node() == ctx.remote_id) {
        CAF_LOG_INFO("incoming connection from self");
        return close_connection;
      }
      else if (!try_set_default_route(ctx.remote_id, ctx.hdl)) {
        CAF_LOG_INFO("multiple incoming connections from the same node");
        return close_connection;
      }
      parent().notify<hook::new_connection_established>(ctx.remote_id );
      break;
    }
    case basp::server_handshake: {
      CAF_REQUIRE(payload != nullptr);
      if (ctx.handshake_data == nullptr) {
        CAF_LOG_INFO("received unexpected server handshake");
        return close_connection;
      }
      if (hdr.operation_data != basp::version) {
        CAF_LOG_INFO("tried to connect to a node with different BASP version");
        return close_connection;
      }
      ctx.remote_id = hdr.source_node;
      ctx.handshake_data->remote_id = hdr.source_node;
      binary_deserializer bd{payload->data(), payload->size(), &m_namespace};
      auto remote_aid = bd.read<uint32_t>();
      auto remote_ifs_size = bd.read<uint32_t>();
      std::set<string> remote_ifs;
      for (uint32_t i = 0; i < remote_ifs_size; ++i) {
        auto str = bd.read<string>();
        remote_ifs.insert(std::move(str));
      }
      auto& ifs = *(ctx.handshake_data->expected_ifs);
      if (!std::includes(ifs.begin(), ifs.end(),
                         remote_ifs.begin(), remote_ifs.end())) {
        auto tostr = [](const std::set<string>& what) -> string {
          if (what.empty()) {
            return "actor";
          }
          string tmp;
          tmp = "typed_actor<";
          auto i = what.begin();
          auto e = what.end();
          tmp += *i++;
          while (i != e) {
            tmp += ",";
            tmp += *i++;
          }
          tmp += ">";
          return tmp;
        };
        auto iface_str = tostr(remote_ifs);
        auto expected_str = tostr(ifs);
        std::string error_msg;
        if (ifs.empty()) {
          error_msg = "expected remote actor to be a "
                      "dynamically typed actor but found "
                      "a strongly typed actor of type "
                      + iface_str;
        }
        else if (remote_ifs.empty()) {
          error_msg = "expected remote actor to be a "
                      "strongly typed actor of type "
                      + expected_str +
                      " but found a dynamically typed actor";
        } else {
          error_msg = "expected remote actor to be a "
                      "strongly typed actor of type "
                      + expected_str +
                      " but found a strongly typed actor of type "
                      + iface_str;
        }
        // abort with error
        std::runtime_error err{error_msg};
        ctx.handshake_data->result->set_exception(std::make_exception_ptr(err));
        return close_connection;
      }
      auto nid = ctx.handshake_data->remote_id;
      if (nid == node()) {
        CAF_LOG_INFO("incoming connection from self: drop connection");
        auto res = detail::singletons::get_actor_registry()->get(remote_aid);
        ctx.handshake_data->result->set_value(std::move(res));
        ctx.handshake_data = nullptr;
        return close_connection;
      }
      if (!try_set_default_route(nid, ctx.hdl)) {
        CAF_LOG_INFO("multiple connections to " << to_string(nid)
                     << " (re-use old one)");
        auto proxy = m_namespace.get_or_put(nid, remote_aid);
        // discard this peer; there's already an open connection
        ctx.handshake_data->result->set_value(std::move(proxy));
        ctx.handshake_data = nullptr;
        return close_connection;
      }
      // finalize handshake
      auto& buf = wr_buf(ctx.hdl);
      binary_serializer bs(std::back_inserter(buf), &m_namespace);
      write(bs, {node(), ctx.handshake_data->remote_id,
                 invalid_actor_id, invalid_actor_id,
                 0, basp::client_handshake, 0});
      // prepare to receive messages
      auto proxy = m_namespace.get_or_put(nid, remote_aid);
      ctx.published_actor = proxy;
      ctx.handshake_data->result->set_value(std::move(proxy));
      ctx.handshake_data = nullptr;
      parent().notify<hook::new_connection_established>(nid);
      break;
    }
  }
  return await_header;
}

void basp_broker::send_kill_proxy_instance(const id_type& nid, actor_id aid,
                                           uint32_t reason) {
  CAF_LOG_TRACE(CAF_TSARG(nid) << ", " << CAF_ARG(aid) << CAF_ARG(reason));
  auto route = get_route(nid);
  CAF_LOG_DEBUG(CAF_MARG(route.hdl, id));
  if (route.invalid()) {
    CAF_LOG_INFO("message dropped, no route to node: " << to_string(nid));
    return;
  }
  auto& buf = wr_buf(route.hdl);
  binary_serializer bs(std::back_inserter(buf), &m_namespace);
  write(bs,
        {node(), nid,                       aid,             invalid_actor_id,
         0,      basp::kill_proxy_instance, uint64_t{reason}});
  flush(route.hdl);
}

basp_broker::connection_info basp_broker::get_route(const id_type& dest) {
  connection_info res;
  auto i = m_routes.find(dest);
  if (i != m_routes.end()) {
    auto& entry = i->second;
    res = entry.first;
    if (res.invalid() && !entry.second.empty()) {
      res = *entry.second.begin();
    }
  }
  return res;
}

actor_proxy_ptr basp_broker::make_proxy(const id_type& nid, actor_id aid) {
  CAF_LOG_TRACE(CAF_TSARG(nid) << ", "
              << CAF_ARG(aid));
  CAF_REQUIRE(m_current_context != nullptr);
  CAF_REQUIRE(aid != invalid_actor_id);
  CAF_REQUIRE(nid != node());
  // this member function is being called whenever we deserialize a
  // payload received from a remote node; if a remote node N sends
  // us a handle to a third node T, we assume that N has a route to T
  if (nid != m_current_context->remote_id) {
    add_route(nid, m_current_context->hdl);
  }
  // we need to tell remote side we are watching this actor now;
  // use a direct route if possible, i.e., when talking to a third node
  auto route = get_route(nid);
  if (route.invalid()) {
    // this happens if and only if we don't have a path to `nid`
    // and m_current_context->hdl has been blacklisted
    CAF_LOG_INFO("cannot create a proxy instance for an actor "
           "running on a node we don't have a route to");
    return nullptr;
  }
  // create proxy and add functor that will be called if we
  // receive a kill_proxy_instance message
  intrusive_ptr<basp_broker> self = this;
  auto mm = middleman::instance();
  auto res = make_counted<remote_actor_proxy>(aid, nid, self);
  res->attach_functor([=](uint32_t) {
    mm->backend().dispatch([=] {
      // using res->id() instead of aid keeps this actor instance alive
      // until the original instance terminates, thus preventing subtle
      // bugs with attachables
      erase_proxy(nid, res->id());
    });
  });
  // tell remote side we are monitoring this actor now
  binary_serializer bs(std::back_inserter(wr_buf(route.hdl)), &m_namespace);
  write(bs, {node(), nid, invalid_actor_id, aid,
         0, basp::announce_proxy_instance, 0});
  // run hooks
  parent().notify<hook::new_remote_actor>(res->address());
  return res;
}

void basp_broker::erase_proxy(const id_type& nid, actor_id aid) {
  CAF_LOGM_TRACE("make_behavior$_DelProxy",
                 CAF_TSARG(nid) << ", " << CAF_ARG(aid));
  m_namespace.erase(nid, aid);
  if (m_namespace.empty()) {
    CAF_LOG_DEBUG("no proxy left, request shutdown of connection");
  }
}

void basp_broker::add_route(const id_type& nid, connection_handle hdl) {
  if (m_blacklist.count(std::make_pair(nid, hdl)) == 0) {
    parent().notify<hook::new_route_added>(m_current_context->remote_id, nid);
    m_routes[nid].second.insert({hdl, nid});
  }
}

bool basp_broker::try_set_default_route(const id_type& nid,
                                        connection_handle hdl) {
  CAF_REQUIRE(!hdl.invalid());
  auto& entry = m_routes[nid];
  if (entry.first.invalid()) {
    CAF_LOG_DEBUG("new default route: " << to_string(nid) << " -> "
                                        << hdl.id());
    entry.first = {hdl, nid};
    return true;
  }
  return false;
}

void basp_broker::init_client(connection_handle hdl,
                              client_handshake_data* data) {
  CAF_LOG_TRACE("hdl = " << hdl.id());
  auto& ctx = m_ctx[hdl];
  ctx.hdl = hdl;
  init_handshake_as_client(ctx, data);
}

void basp_broker::init_handshake_as_client(connection_context& ctx,
                                           client_handshake_data* ptr) {
  CAF_LOG_TRACE(CAF_ARG(this));
  ctx.state = await_server_handshake;
  ctx.handshake_data = ptr;
  configure_read(ctx.hdl, receive_policy::exactly(basp::header_size));
}

void basp_broker::init_handshake_as_sever(connection_context& ctx,
                                          actor_addr addr) {
  CAF_LOG_TRACE(CAF_ARG(this));
  CAF_REQUIRE(node() != invalid_node_id);
  auto& buf = wr_buf(ctx.hdl);
  auto wrpos = buf.size();
  char padding[basp::header_size];
  buf.insert(buf.end(), std::begin(padding), std::end(padding));
  auto before = buf.size();
  { // lifetime scope of first serializer
    binary_serializer bs1(std::back_inserter(buf), &m_namespace);
    bs1 << addr.id();
    auto sigs = addr.message_types();
    bs1 << static_cast<uint32_t>(sigs.size());
    for (auto& sig : sigs) {
      bs1 << sig;
    }
  }
  // fill padded region with the actual broker message
  binary_serializer bs2(buf.begin() + wrpos, &m_namespace);
  auto payload_len = buf.size() - before;
  write(bs2, {node(),                             invalid_node_id,
              addr.id(),                          0,
              static_cast<uint32_t>(payload_len), basp::server_handshake,
              basp::version});
  CAF_LOG_DEBUG("buf.size() " << buf.size());
  flush(ctx.hdl);
  // setup for receiving client handshake
  ctx.state = await_client_handshake;
  configure_read(ctx.hdl, receive_policy::exactly(basp::header_size));
}

void basp_broker::add_published_actor(accept_handle hdl,
                                      const abstract_actor_ptr& ptr,
                                      uint16_t port) {
  CAF_LOG_TRACE("");
  if (!ptr) {
    return;
  }
  CAF_LOG_TRACE("");
  m_acceptors.insert(std::make_pair(hdl, std::make_pair(ptr, port)));
  m_open_ports.insert(std::make_pair(port, hdl));
  ptr->attach_functor([port](abstract_actor* self, uint32_t) {
    unpublish_impl(self, port, false);
  });
  if (ptr->node() == node()) {
    singletons::get_actor_registry()->put(ptr->id(), ptr);
  }
}

void basp_broker::remove_published_actor(const abstract_actor_ptr& whom,
                                         uint16_t port) {
  CAF_LOG_TRACE("");
  auto i = m_open_ports.find(port);
  if (i == m_open_ports.end()) {
    return;
  }
  auto j = m_acceptors.find(i->second);
  if (j == m_acceptors.end()) {
    CAF_LOG_ERROR("accept handle for port " << port
                  << " not found in m_published_actors");
    m_open_ports.erase(i);
    return;
  }
  if (j->second.first != whom) {
    CAF_LOG_INFO("port has been bound to a different actor already");
    return;
  }
  close(j->first);
  m_open_ports.erase(i);
  m_acceptors.erase(j);
}

} // namespace io
} // namespace caf
