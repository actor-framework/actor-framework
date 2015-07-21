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

#include "caf/exception.hpp"
#include "caf/make_counted.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/forwarding_actor_proxy.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

#include "caf/io/basp.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/unpublish.hpp"

using std::string;

namespace caf {
namespace io {

using detail::singletons;

basp_broker::payload_writer::~payload_writer() {
  // nop
}

template <class F>
class functor_payload_writer : public basp_broker::payload_writer {
public:
  explicit functor_payload_writer(F fun) : fun_(fun) {
    // nop
  }

  functor_payload_writer(const functor_payload_writer&) = default;

  functor_payload_writer& operator=(const functor_payload_writer&) = default;

  void write(binary_serializer& sink) override {
    fun_(sink);
  }

private:
  F fun_;
};

template <class F>
functor_payload_writer<F> make_payload_writer(F fun) {
  return functor_payload_writer<F>{fun};
}

basp_broker::basp_broker(middleman& pref)
    : broker(pref),
      namespace_(*this),
      current_context_(nullptr) {
  meta_msg_ = uniform_typeid<message>();
  meta_id_type_ = uniform_typeid<node_id>();
  CAF_LOG_DEBUG("BASP broker started: " << to_string(node()));
}

basp_broker::~basp_broker() {
  CAF_LOG_TRACE("");
}

behavior basp_broker::make_behavior() {
  return {
    // received from underlying broker implementation
    [=](new_data_msg& msg) {
      CAF_LOG_TRACE("handle = " << msg.handle.id());
      CAF_ASSERT(ctx_.count(msg.handle) > 0);
      new_data(ctx_[msg.handle], msg.buf);
    },
    // received from underlying broker implementation
    [=](const new_connection_msg& msg) {
      CAF_LOG_TRACE("handle = " << msg.handle.id());
      CAF_ASSERT(ctx_.count(msg.handle) == 0);
      auto& ctx = ctx_[msg.handle];
      ctx.hdl = msg.handle;
      ctx.handshake_data = none;
      ctx.state = await_client_handshake;
      init_handshake_as_server(ctx, acceptors_[msg.source].first->address());
    },
    // received from underlying broker implementation
    [=](const connection_closed_msg& msg) {
      CAF_LOG_TRACE(CAF_MARG(msg.handle, id));
      auto j = ctx_.find(msg.handle);
      if (j != ctx_.end()) {
        auto hd = j->second.handshake_data;
        if (hd) {
          send(hd->client, error_atom::value, hd->request_id,
               "disconnect during handshake");
        }
        ctx_.erase(j);
      }
      // purge handle from all routes
      std::vector<node_id> lost_connections;
      for (auto& kvp : routes_) {
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
        fail_pending_requests(lc, exit_reason::remote_link_unreachable);
        routes_.erase(lc);
        auto proxies = namespace_.get_all(lc);
        namespace_.erase(lc);
        for (auto& p : proxies) {
          p->kill_proxy(exit_reason::remote_link_unreachable);
        }
      }
    },
    // received from underlying broker implementation
    [=](const acceptor_closed_msg& msg) {
      CAF_LOG_TRACE("");
      auto i = acceptors_.find(msg.handle);
      if (i == acceptors_.end()) {
        CAF_LOG_INFO("accept handle no longer in use");
        return;
      }
      if (open_ports_.erase(i->second.second) == 0) {
        CAF_LOG_INFO("accept handle was not bound to a port");
      }
      acceptors_.erase(i);
    },
    // received from proxy instances
    [=](forward_atom, const actor_addr& sender, const actor_addr& receiver,
        message_id mid, const message& msg) {
      CAF_LOG_TRACE("");
      if (dispatch(sender, receiver, mid, msg) == invalid_node_id
          && mid.is_request()) {
        detail::sync_request_bouncer srb{exit_reason::remote_link_unreachable};
        srb(sender, mid);
      }
    },
    [=](delete_atom, const node_id& nid, actor_id aid) {
      CAF_LOG_TRACE(CAF_TSARG(nid) << ", " << CAF_ARG(aid));
      erase_proxy(nid, aid);
    },
    // received from middleman actor
    [=](put_atom, accept_handle hdl, const actor_addr& whom, uint16_t port) {
      CAF_LOG_TRACE(CAF_ARG(hdl.id()) << ", "<< CAF_TSARG(whom)
                    << ", " << CAF_ARG(port));
      if (hdl.invalid() || whom == invalid_actor_addr) {
        return;
      }
      try {
        assign_tcp_doorman(hdl);
      }
      catch (...) {
        CAF_LOG_DEBUG("failed to assign doorman from handle");
        return;
      }
      add_published_actor(hdl, actor_cast<abstract_actor_ptr>(whom), port);
      parent().notify<hook::actor_published>(whom, port);
    },
    [=](get_atom, connection_handle hdl, int64_t request_id,
        actor client, std::set<std::string>& expected_ifs) {
      CAF_LOG_TRACE(CAF_ARG(hdl.id()) << ", " << CAF_ARG(request_id)
                    << ", " << CAF_TSARG(client));
      try {
        assign_tcp_scribe(hdl);
      }
      catch (std::exception& e) {
        CAF_LOG_DEBUG("failed to assign scribe from handle: " << e.what());
        send(client, error_atom::value, request_id,
             std::string("failed to assign scribe from handle: ") + e.what());
        return;
      }
      auto& ctx = ctx_[hdl];
      ctx.hdl = hdl;
      // PODs are not movable, so passing expected_ifs to the ctor  would cause
      // a copy; we avoid this by calling the ctor with an empty set and
      // swap afterwards with expected_ifs
      ctx.handshake_data = client_handshake_data{request_id, client,
                                                 std::set<std::string>()};
      ctx.handshake_data->expected_ifs.swap(expected_ifs);
      init_handshake_as_client(ctx);
    },
    [=](delete_atom, int64_t request_id, const actor_addr& whom, uint16_t port)
    -> message {
      CAF_LOG_TRACE(CAF_ARG(request_id) << ", " << CAF_TSARG(whom)
                    << ", " << CAF_ARG(port));
      if (whom == invalid_actor_addr) {
        return make_message(error_atom::value, request_id,
                            "whom == invalid_actor_addr");
      }
      auto ptr = actor_cast<abstract_actor_ptr>(whom);
      if (port == 0) {
        if (! remove_published_actor(ptr)) {
          return make_message(error_atom::value, request_id,
                              "no mapping found");
        }
      } else {
        if (! remove_published_actor(ptr, port)) {
          return make_message(error_atom::value, request_id,
                              "port not bound to actor");
        }
      }
      return make_message(ok_atom::value, request_id);
    },
    // catch-all error handler
    others >> [=] {
      CAF_LOG_ERROR("received unexpected message: "
                   << to_string(current_message()));
    }
  };
}

void basp_broker::new_data(connection_context& ctx, buffer_type& buf) {
  CAF_LOG_TRACE(CAF_TARG(ctx.state, static_cast<int>) << ", "
                CAF_MARG(ctx.hdl, id));
  current_context_ = &ctx;
  connection_state next_state;
  switch (ctx.state) {
    default: {
      binary_deserializer bd{buf.data(), buf.size(), &namespace_};
      read(bd, ctx.hdr);
      if (! basp::valid(ctx.hdr)) {
        CAF_LOG_INFO("invalid broker message received");
        close(ctx.hdl);
        ctx_.erase(ctx.hdl);
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
    ctx_.erase(ctx.hdl);
    return;
  }
  ctx.state = next_state;
  configure_read(ctx.hdl, receive_policy::exactly(next_state == await_payload
                                                    ? ctx.hdr.payload_len
                                                    : basp::header_size));
}

void basp_broker::local_dispatch(const basp::header& hdr, message&& msg) {
  // TODO: provide hook API to allow ActorShell to
  //       intercept/trace/log each message
  CAF_LOG_TRACE("");
  // look up the source of the messages
  actor_addr src;
  if (hdr.source_node != invalid_node_id
      && hdr.source_actor != invalid_actor_id) {
    if (hdr.source_node != node()) {
      CAF_LOG_DEBUG("source is a proxy");
      src = namespace_.get_or_put(hdr.source_node,
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
  CAF_ASSERT(! dest || dest->node() == node());
  // intercept message used for link signaling
  if (dest && src == dest) {
    if (msg.match_elements<atom_value, actor_addr>()) {
      actor_addr other;
      bool is_unlink = true;
      // extract arguments
      msg.apply({
        [&](link_atom, const actor_addr& addr) {
          is_unlink = false;
          other = addr;
        },
        [&](unlink_atom, const actor_addr& addr) {
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
  if (! dest) {
    CAF_LOG_DEBUG("received a message for an invalid actor; "
                  "could not find an actor with ID "
                  << hdr.dest_actor);
    parent().notify<hook::invalid_message_received>(hdr.source_node, src,
                                                    hdr.dest_actor, mid, msg);
    return;
  }
  auto dest_addr = dest->address();
  if (mid.is_response() && ! pending_requests_.empty()) {
    // remove from pendings requests
    auto req_id = mid.request_id();
    auto last = pending_requests_.end();
    auto i = std::find(pending_requests_.begin(), last,
                       std::tie(hdr.source_node, dest_addr, req_id));
    if (i != last) {
      if (i != (last - 1)) {
        using std::swap;
        swap(*i, pending_requests_.back());
      }
      pending_requests_.pop_back();
    }
  }
  parent().notify<hook::message_received>(hdr.source_node, src,
                                          dest_addr, mid, msg);
  CAF_LOG_DEBUG("enqueue message from " << to_string(src) << " to "
                << to_string(dest->address()));
  dest->enqueue(src, mid, std::move(msg), nullptr);
}

void basp_broker::dispatch(connection_handle hdl, uint32_t operation,
                           const node_id& src_node, actor_id src_actor,
                           const node_id& dest_node, actor_id dest_actor,
                           uint64_t op_data, payload_writer* writer) {
  auto& buf = wr_buf(hdl);
  if (writer) {
    // reserve space in the buffer to write the broker message later on
    auto wr_pos = static_cast<ptrdiff_t>(buf.size());
    char placeholder[basp::header_size];
    buf.insert(buf.end(), std::begin(placeholder), std::end(placeholder));
    auto before = buf.size();
    { // lifetime scope of first serializer (write payload)
      binary_serializer bs1{std::back_inserter(buf), &namespace_};
      writer->write(bs1);
    }
    // write broker message to the reserved space
    binary_serializer bs2{buf.begin() + wr_pos, &namespace_};
    auto payload_len = static_cast<uint32_t>(buf.size() - before);
    write(bs2, {src_node,    dest_node, src_actor,   dest_actor,
                payload_len, operation, op_data});
  } else {
    binary_serializer bs(std::back_inserter(wr_buf(hdl)), &namespace_);
    write(bs, {src_node, dest_node, src_actor, dest_actor,
               0, operation, op_data});
  }
  flush(hdl);
}

node_id basp_broker::dispatch(uint32_t operation, const node_id& src_node,
                              actor_id src_actor, const node_id& dest_node,
                              actor_id dest_actor, uint64_t op_data,
                              payload_writer* writer) {
  auto route = get_route(dest_node);
  if (route.invalid()) {
    CAF_LOG_INFO("unable to dispatch message: no route to "
                 << CAF_TSARG(dest_node));
    return invalid_node_id;
  }
  dispatch(route.hdl, operation, src_node, src_actor, dest_node, dest_actor,
           op_data, writer);
  return route.node;
}

node_id basp_broker::dispatch(const actor_addr& from, const actor_addr& to,
                              message_id mid, const message& msg) {
  CAF_LOG_TRACE(CAF_TSARG(from) << ", " << CAF_MARG(mid, integer_value)
                << ", " << CAF_TSARG(to) << ", " << CAF_TSARG(msg));
  if (to == invalid_actor_addr) {
    return invalid_node_id;
  }
  if (from != invalid_actor_addr && from.node() == node()) {
    // register locally running actors to be able to deserialize them later
    auto reg = detail::singletons::get_actor_registry();
    reg->put(from.id(), actor_cast<abstract_actor_ptr>(from));
  }
  auto writer = make_payload_writer([&](binary_serializer& sink) {
    sink.write(msg, meta_msg_);
  });
  auto route_node = dispatch(basp::dispatch_message, from.node(), from.id(),
                             to.node(), to.id(), mid.integer_value(), &writer);
  if (route_node == invalid_node_id) {
    parent().notify<hook::message_sending_failed>(from, to, mid, msg);
  } else {
    if (mid.is_request()) {
      // keep track of pendings sync requests for error handling
      pending_requests_.emplace_back(to.node(), from, mid);
    }
    parent().notify<hook::message_sent>(from, route_node, to, mid, msg);
  }
  return route_node;
}

void basp_broker::read(binary_deserializer& bd, basp::header& msg) {
  bd.read(msg.source_node, meta_id_type_)
    .read(msg.dest_node, meta_id_type_)
    .read(msg.source_actor)
    .read(msg.dest_actor)
    .read(msg.payload_len)
    .read(msg.operation)
    .read(msg.operation_data);
}

void basp_broker::write(binary_serializer& bs, const basp::header& msg) {
  bs.write(msg.source_node, meta_id_type_)
    .write(msg.dest_node, meta_id_type_)
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
  if (! payload && hdr.payload_len > 0) {
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
    binary_serializer bs{std::back_inserter(buf), &namespace_};
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
      CAF_ASSERT(payload != nullptr);
      binary_deserializer bd{payload->data(), payload->size(), &namespace_};
      message content;
      bd.read(content, meta_msg_);
      local_dispatch(ctx.hdr, std::move(content));
      break;
    }
    case basp::announce_proxy_instance: {
      CAF_ASSERT(payload == nullptr);
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
            CAF_LOG_TRACE(CAF_ARG(reason));
            auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
            bro->send_kill_proxy_instance(nid, aid, reason);
          });
        });
      }
      break;
    }
    case basp::kill_proxy_instance: {
      CAF_ASSERT(payload == nullptr);
      // we have a proxy to an actor that has been terminated
      auto ptr = namespace_.get(hdr.source_node, hdr.source_actor);
      if (ptr) {
        namespace_.erase(ptr->node(), ptr->id());
        ptr->kill_proxy(static_cast<uint32_t>(hdr.operation_data));
      } else {
        CAF_LOG_DEBUG("received kill proxy twice");
      }
      break;
    }
    case basp::client_handshake: {
      CAF_ASSERT(payload == nullptr);
      if (ctx.remote_id != invalid_node_id) {
        CAF_LOG_INFO("received unexpected client handshake");
        return close_connection;
      }
      ctx.remote_id = hdr.source_node;
      if (node() == ctx.remote_id) {
        CAF_LOG_INFO("incoming connection from self");
        return close_connection;
      }
      else if (! try_set_default_route(ctx.remote_id, ctx.hdl)) {
        CAF_LOG_INFO("multiple incoming connections from the same node");
        return close_connection;
      }
      parent().notify<hook::new_connection_established>(ctx.remote_id );
      break;
    }
    case basp::server_handshake: {
      CAF_ASSERT(payload != nullptr);
      if (! ctx.handshake_data) {
        CAF_LOG_INFO("received unexpected server handshake");
        return close_connection;
      }
      if (hdr.operation_data != basp::version) {
        CAF_LOG_INFO("tried to connect to a node with different BASP version");
        return close_connection;
      }
      ctx.remote_id = hdr.source_node;
      binary_deserializer bd{payload->data(), payload->size(), &namespace_};
      auto remote_aid = bd.read<uint32_t>();
      auto remote_ifs_size = bd.read<uint32_t>();
      std::set<string> remote_ifs;
      for (uint32_t i = 0; i < remote_ifs_size; ++i) {
        auto str = bd.read<string>();
        remote_ifs.insert(std::move(str));
      }
      auto& ifs = ctx.handshake_data->expected_ifs;
      auto hsclient = ctx.handshake_data->client;
      auto hsid = ctx.handshake_data->request_id;
      if (! std::includes(ifs.begin(), ifs.end(),
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
        send(hsclient, error_atom::value, hsid, std::move(error_msg));
        return close_connection;
      }
      auto nid = hdr.source_node;
      if (nid == node()) {
        CAF_LOG_INFO("incoming connection from self: drop connection");
        auto res = detail::singletons::get_actor_registry()->get(remote_aid);
        send(hsclient, ok_atom::value, hsid, res->address());
        ctx.handshake_data = none;
        return close_connection;
      }
      if (! try_set_default_route(nid, ctx.hdl)) {
        CAF_LOG_INFO("multiple connections to " << to_string(nid)
                     << " (re-use old one)");
        auto proxy = namespace_.get_or_put(nid, remote_aid);
        // discard this peer; there's already an open connection
        send(hsclient, ok_atom::value, hsid, proxy->address());
        ctx.handshake_data = none;
        return close_connection;
      }
      // finalize handshake
      dispatch(ctx.hdl, basp::client_handshake,
               node(), invalid_actor_id, nid, invalid_actor_id);
      // prepare to receive messages
      auto proxy = namespace_.get_or_put(nid, remote_aid);
      ctx.published_actor = proxy;
      send(hsclient, ok_atom::value, hsid, proxy->address());
      ctx.handshake_data = none;
      parent().notify<hook::new_connection_established>(nid);
      break;
    }
  }
  return await_header;
}

void basp_broker::send_kill_proxy_instance(const node_id& nid, actor_id aid,
                                           uint32_t reason) {
  CAF_LOG_TRACE(CAF_TSARG(nid) << ", " << CAF_ARG(aid) << CAF_ARG(reason));
  auto route_node = dispatch(basp::kill_proxy_instance, node(), aid,
                             nid, invalid_actor_id, reason);
  if (route_node == invalid_node_id) {
    CAF_LOG_INFO("message dropped, no route to node: " << to_string(nid));
  }
}

basp_broker::connection_info basp_broker::get_route(const node_id& dest) {
  connection_info res;
  auto i = routes_.find(dest);
  if (i != routes_.end()) {
    auto& entry = i->second;
    res = entry.first;
    if (res.invalid() && ! entry.second.empty()) {
      res = *entry.second.begin();
    }
  }
  return res;
}

actor_proxy_ptr basp_broker::make_proxy(const node_id& nid, actor_id aid) {
  CAF_LOG_TRACE(CAF_TSARG(nid) << ", "
              << CAF_ARG(aid));
  CAF_ASSERT(current_context_ != nullptr);
  CAF_ASSERT(aid != invalid_actor_id);
  CAF_ASSERT(nid != node());
  // this member function is being called whenever we deserialize a
  // payload received from a remote node; if a remote node N sends
  // us a handle to a third node T, we assume that N has a route to T
  if (nid != current_context_->remote_id) {
    add_route(nid, current_context_->hdl);
  }
  // we need to tell remote side we are watching this actor now;
  // use a direct route if possible, i.e., when talking to a third node
  auto route = get_route(nid);
  if (route.invalid()) {
    // this happens if and only if we don't have a path to `nid`
    // and current_context_->hdl has been blacklisted
    CAF_LOG_INFO("cannot create a proxy instance for an actor "
           "running on a node we don't have a route to");
    return nullptr;
  }
  // create proxy and add functor that will be called if we
  // receive a kill_proxy_instance message
  intrusive_ptr<basp_broker> self = this;
  auto mm = middleman::instance();
  auto res = make_counted<forwarding_actor_proxy>(aid, nid, self);
  res->attach_functor([=](uint32_t) {
    mm->backend().dispatch([=] {
      // using res->id() instead of aid keeps this actor instance alive
      // until the original instance terminates, thus preventing subtle
      // bugs with attachables
      erase_proxy(nid, res->id());
    });
  });
  // tell remote side we are monitoring this actor now
  dispatch(route.hdl, basp::announce_proxy_instance,
           node(), invalid_actor_id, nid, aid);
  parent().notify<hook::new_remote_actor>(res->address());
  return res;
}

void basp_broker::on_exit() {
  CAF_LOG_TRACE("");
  // kill all remaining proxies
  auto proxies = namespace_.get_all();
  for (auto& proxy : proxies) {
    CAF_ASSERT(proxy != nullptr);
    proxy->kill_proxy(exit_reason::remote_link_unreachable);
  }
  namespace_.clear();
  // remove all remaining state
  ctx_.clear();
  acceptors_.clear();
  open_ports_.clear();
  routes_.clear();
  blacklist_.clear();
  pending_requests_.clear();
}

void basp_broker::erase_proxy(const node_id& nid, actor_id aid) {
  CAF_LOG_TRACE(CAF_TSARG(nid) << ", " << CAF_ARG(aid));
  namespace_.erase(nid, aid);
  if (namespace_.empty()) {
    CAF_LOG_DEBUG("no proxy left, request shutdown of connection");
  }
}

void basp_broker::add_route(const node_id& nid, connection_handle hdl) {
  if (blacklist_.count(std::make_pair(nid, hdl)) == 0) {
    parent().notify<hook::new_route_added>(current_context_->remote_id, nid);
    routes_[nid].second.insert({hdl, nid});
  }
}

bool basp_broker::try_set_default_route(const node_id& nid,
                                        connection_handle hdl) {
  CAF_ASSERT(! hdl.invalid());
  auto& entry = routes_[nid];
  if (entry.first.invalid()) {
    CAF_LOG_DEBUG("new default route: " << to_string(nid) << " -> "
                                        << hdl.id());
    entry.first = {hdl, nid};
    return true;
  }
  return false;
}

void basp_broker::init_handshake_as_client(connection_context& ctx) {
  CAF_LOG_TRACE(CAF_ARG(this));
  ctx.state = await_server_handshake;
  configure_read(ctx.hdl, receive_policy::exactly(basp::header_size));
}

void basp_broker::init_handshake_as_server(connection_context& ctx,
                                           actor_addr addr) {
  CAF_LOG_TRACE(CAF_ARG(this));
  CAF_ASSERT(node() != invalid_node_id);
  if (addr != invalid_actor_addr) {
    auto writer = make_payload_writer([&](binary_serializer& sink) {
      sink << addr.id();
      // TODO: this always exposes the "real" type of an actor,
      //       which prohibits users from announcing sub-types
      auto sigs = addr->message_types();
      sink << static_cast<uint32_t>(sigs.size());
      for (auto& sig : sigs) {
        sink << sig;
      }
    });
    dispatch(ctx.hdl, basp::server_handshake, node(), addr.id(),
             invalid_node_id, invalid_actor_id, basp::version, &writer);
  } else {
    dispatch(ctx.hdl, basp::server_handshake, node(), invalid_actor_id,
             invalid_node_id, invalid_actor_id, basp::version);
  }
  // prepare for receiving client handshake
  ctx.state = await_client_handshake;
  configure_read(ctx.hdl, receive_policy::exactly(basp::header_size));
}

void basp_broker::add_published_actor(accept_handle hdl,
                                      const abstract_actor_ptr& ptr,
                                      uint16_t port) {
  CAF_LOG_TRACE("");
  if (! ptr) {
    return;
  }
  acceptors_.emplace(hdl, std::make_pair(ptr, port));
  open_ports_.emplace(port, hdl);
  ptr->attach_functor([port](abstract_actor* self, uint32_t) {
    unpublish_impl(self->address(), port, false);
  });
  if (ptr->node() == node()) {
    singletons::get_actor_registry()->put(ptr->id(), ptr);
  }
}

bool basp_broker::remove_published_actor(const abstract_actor_ptr& whom) {
  CAF_LOG_TRACE("");
  CAF_ASSERT(whom != nullptr);
  size_t erased_elements = 0;
  auto last = acceptors_.end();
  auto i = acceptors_.begin();
  while (i != last) {
    auto& kvp = i->second;
    if (kvp.first == whom) {
      CAF_ASSERT(valid(i->first));
      close(i->first);
      if (open_ports_.erase(kvp.second) == 0) {
        CAF_LOG_ERROR("inconsistent data: no open port for acceptor!");
      }
      i = acceptors_.erase(i);
      ++erased_elements;
    }
    else {
      ++i;
    }
  }
  return erased_elements > 0;
}

bool basp_broker::remove_published_actor(const abstract_actor_ptr& whom,
                                         uint16_t port) {
  CAF_LOG_TRACE("");
  CAF_ASSERT(whom != nullptr);
  CAF_ASSERT(port != 0);
  auto i = open_ports_.find(port);
  if (i == open_ports_.end()) {
    return false;
  }
  CAF_ASSERT(valid(i->second));
  auto j = acceptors_.find(i->second);
  if (j->second.first != whom) {
    CAF_LOG_INFO("port has been bound to a different actor");
    return false;
  }
  close(i->second);
  open_ports_.erase(i);
  if (j == acceptors_.end()) {
    CAF_LOG_ERROR("inconsistent data: accept handle for port " << port
                  << " not found in published_actors_");
  } else {
    acceptors_.erase(j);
  }
  return true;
}

void basp_broker::fail_pending_requests(pending_request_iter first,
                                        pending_request_iter last,
                                        uint32_t reason) {
  CAF_LOG_TRACE(std::distance(first, last) << " elements, " << CAF_ARG(reason));
  if (first == last) {
    return;
  }
  detail::sync_request_bouncer srb{reason};
  auto bounce = [&](const pending_request& req) {
    srb(get<1>(req), get<2>(req));
  };
  std::for_each(first, last, bounce);
  pending_requests_.erase(first, last);
}

void basp_broker::fail_pending_requests(uint32_t reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  fail_pending_requests(pending_requests_.begin(), pending_requests_.end(),
                        reason);
}

void basp_broker::fail_pending_requests(const node_id& addr, uint32_t reason) {
  CAF_LOG_TRACE(CAF_TSARG(addr) << ", " << CAF_ARG(reason));
  auto f = [&](const pending_request& req) {
    return get<0>(req) == addr;
  };
  auto last = pending_requests_.end();
  fail_pending_requests(std::remove_if(pending_requests_.begin(), last, f),
                        last, reason);
}

} // namespace io
} // namespace caf
