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

#include "caf/io/basp/instance.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/defaults.hpp"
#include "caf/io/basp/version.hpp"
#include "caf/streambuf.hpp"

namespace caf {
namespace io {
namespace basp {

instance::callee::callee(actor_system& sys, proxy_registry::backend& backend)
    : namespace_(sys, backend) {
  // nop
}

instance::callee::~callee() {
  // nop
}

instance::instance(abstract_broker* parent, callee& lstnr)
    : tbl_(parent),
      this_node_(parent->system().node()),
      callee_(lstnr) {
  CAF_ASSERT(this_node_ != none);
}

connection_state instance::handle(execution_unit* ctx,
                                  new_data_msg& dm, header& hdr,
                                  bool is_payload) {
  CAF_LOG_TRACE(CAF_ARG(dm) << CAF_ARG(is_payload));
  // function object providing cleanup code on errors
  auto err = [&]() -> connection_state {
    auto cb = make_callback([&](const node_id& nid) -> error {
      callee_.purge_state(nid);
      return none;
    });
    tbl_.erase(dm.handle, cb);
    return close_connection;
  };
  std::vector<char>* payload = nullptr;
  if (is_payload) {
    payload = &dm.buf;
    if (payload->size() != hdr.payload_len) {
      CAF_LOG_WARNING("received invalid payload, expected"
                      << hdr.payload_len << "bytes, got" << payload->size());
      return err();
    }
  } else {
    binary_deserializer bd{ctx, dm.buf};
    auto e = bd(hdr);
    if (e || !valid(hdr)) {
      CAF_LOG_WARNING("received invalid header:" << CAF_ARG(hdr));
      return err();
    }
    if (hdr.payload_len > 0) {
      CAF_LOG_DEBUG("await payload before processing further");
      return await_payload;
    }
  }
  CAF_LOG_DEBUG(CAF_ARG(hdr));
  if (!handle(ctx, dm.handle, hdr, payload))
    return err();
  return await_header;
}

void instance::handle_heartbeat(execution_unit* ctx) {
  CAF_LOG_TRACE("");
  for (auto& kvp: tbl_.nid_by_hdl_) {
    CAF_LOG_TRACE(CAF_ARG(kvp.first) << CAF_ARG(kvp.second));
    write_heartbeat(ctx, callee_.get_buffer(kvp.first));
    callee_.flush(kvp.first);
  }
}

routing_table::lookup_result instance::lookup(const node_id& target) {
  return tbl_.lookup(target);
}

void instance::flush(connection_handle hdl) {
  callee_.flush(hdl);
}

void instance::write(execution_unit* ctx, connection_handle hdl,
                     header& hdr, payload_writer* writer) {
  CAF_LOG_TRACE(CAF_ARG(hdr));
  CAF_ASSERT(hdr.payload_len == 0 || writer != nullptr);
  write(ctx, callee_.get_buffer(hdl), hdr, writer);
  flush(hdl);
}

void instance::add_published_actor(uint16_t port,
                                   strong_actor_ptr published_actor,
                                   std::set<std::string> published_interface) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(published_actor)
                << CAF_ARG(published_interface));
  using std::swap;
  auto& entry = published_actors_[port];
  swap(entry.first, published_actor);
  swap(entry.second, published_interface);
  notify<hook::actor_published>(entry.first, entry.second, port);
}

size_t instance::remove_published_actor(uint16_t port,
                                        removed_published_actor* cb) {
  CAF_LOG_TRACE(CAF_ARG(port));
  auto i = published_actors_.find(port);
  if (i == published_actors_.end())
    return 0;
  if (cb != nullptr)
    (*cb)(i->second.first, i->first);
  published_actors_.erase(i);
  return 1;
}

size_t instance::remove_published_actor(const actor_addr& whom,
                                        uint16_t port,
                                        removed_published_actor* cb) {
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(port));
  size_t result = 0;
  if (port != 0) {
    auto i = published_actors_.find(port);
    if (i != published_actors_.end() && i->second.first == whom) {
      if (cb != nullptr)
        (*cb)(i->second.first, port);
      published_actors_.erase(i);
      result = 1;
    }
  } else {
    auto i = published_actors_.begin();
    while (i != published_actors_.end()) {
      if (i->second.first == whom) {
        if (cb != nullptr)
          (*cb)(i->second.first, i->first);
        i = published_actors_.erase(i);
        ++result;
      } else {
        ++i;
      }
    }
  }
  return result;
}

bool instance::dispatch(execution_unit* ctx, const strong_actor_ptr& sender,
                        const std::vector<strong_actor_ptr>& forwarding_stack,
                        const strong_actor_ptr& receiver, message_id mid,
                        const message& msg) {
  CAF_LOG_TRACE(CAF_ARG(sender) << CAF_ARG(receiver)
                << CAF_ARG(mid) << CAF_ARG(msg));
  CAF_ASSERT(receiver && system().node() != receiver->node());
  auto lr = lookup(receiver->node());
  if (!lr.known) {
    notify<hook::message_sending_failed>(sender, receiver, mid, msg);
    return false;
  }
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<std::vector<strong_actor_ptr>&>(forwarding_stack),
                const_cast<message&>(msg));
  });
  header hdr{message_type::dispatch_message, 0, 0, mid.integer_value(),
             sender ? sender->id() : invalid_actor_id, receiver->id()};
  if (lr.hdl) {
    auto hdl = std::move(*lr.hdl);
    write(ctx, callee_.get_buffer(hdl), hdr, &writer);
    flush(hdl);
  } else {
    write(ctx, callee_.get_buffer(receiver->node()), hdr, &writer);
  }
  notify<hook::message_sent>(sender, receiver->node(), receiver, mid, msg);
  return true;
}

void instance::write(execution_unit* ctx, buffer_type& buf,
                     header& hdr, payload_writer* pw) {
  CAF_LOG_TRACE(CAF_ARG(hdr));
  error err;
  if (pw != nullptr) {
    auto pos = buf.size();
    // write payload first (skip first 72 bytes and write header later)
    char placeholder[basp::header_size];
    buf.insert(buf.end(), std::begin(placeholder), std::end(placeholder));
    binary_serializer bs{ctx, buf};
    (*pw)(bs);
    auto plen = buf.size() - pos - basp::header_size;
    CAF_ASSERT(plen <= std::numeric_limits<uint32_t>::max());
    hdr.payload_len = static_cast<uint32_t>(plen);
    stream_serializer<charbuf> out{ctx, buf.data() + pos, basp::header_size};
    err = out(hdr);
  } else {
    binary_serializer bs{ctx, buf};
    err = bs(hdr);
  }
  if (err)
    CAF_LOG_ERROR(CAF_ARG(err));
}

void instance::write_server_handshake(execution_unit* ctx, buffer_type& out_buf,
                                      optional<uint16_t> port) {
  CAF_LOG_TRACE(CAF_ARG(port));
  using namespace detail;
  published_actor* pa = nullptr;
  if (port) {
    auto i = published_actors_.find(*port);
    if (i != published_actors_.end())
      pa = &i->second;
  }
  CAF_LOG_DEBUG_IF(!pa && port, "no actor published");
  auto writer = make_callback([&](serializer& sink) -> error {
    auto appid = get_or(callee_.config(), "middleman.app-identifier",
                     defaults::middleman::app_identifier);
    if (auto err = sink(this_node_, appid, tbl_.autoconnect_endpoint()))
      return err;
    if (pa != nullptr) {
      auto i = pa->first ? pa->first->id() : invalid_actor_id;
      return sink(i, pa->second);
    }
    auto aid = invalid_actor_id;
    std::set<std::string> tmp;
    return sink(aid, tmp);
  });
  header hdr{message_type::server_handshake, 0, 0, version,
             (pa != nullptr) && pa->first ? pa->first->id() : invalid_actor_id,
             invalid_actor_id};
  write(ctx, out_buf, hdr, &writer);
}

void instance::write_client_handshake(execution_unit* ctx,
                                      buffer_type& buf,
                                      const node_id& this_node,
                                      const std::string& app_identifier) {
  CAF_LOG_TRACE(CAF_ARG(this_node) << CAF_ARG(app_identifier));
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<node_id&>(this_node),
                const_cast<std::string&>(app_identifier),
                tbl_.autoconnect_endpoint());
  });
  header hdr{message_type::client_handshake, 0, 0, 0, invalid_actor_id,
             invalid_actor_id};
  write(ctx, buf, hdr, &writer);
}

void instance::write_client_handshake(execution_unit* ctx, buffer_type& buf) {
  write_client_handshake(ctx, buf, this_node_,
                         get_or(callee_.config(), "middleman.app-identifier",
                                defaults::middleman::app_identifier));
}

void instance::write_announce_proxy(execution_unit* ctx, buffer_type& buf,
                                    const node_id& dest_node, actor_id aid) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(aid));
  header hdr{message_type::announce_proxy, 0, 0, 0, invalid_actor_id, aid};
  write(ctx, buf, hdr);
}

void instance::write_kill_proxy(execution_unit* ctx, buffer_type& buf,
                                const node_id& dest_node, actor_id aid,
                                const error& rsn) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(aid) << CAF_ARG(rsn));
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<error&>(rsn));
  });
  header hdr{message_type::kill_proxy, 0, 0, 0, aid, invalid_actor_id};
  write(ctx, buf, hdr, &writer);
}

void instance::write_heartbeat(execution_unit* ctx, buffer_type& buf) {
  CAF_LOG_TRACE("");
  header hdr{message_type::heartbeat, 0, 0, 0, invalid_actor_id,
             invalid_actor_id};
  write(ctx, buf, hdr);
}

bool instance::handle(execution_unit* ctx, connection_handle hdl, header& hdr,
                      std::vector<char>* payload) {
  // function object for checking payload validity
  auto payload_valid = [&]() -> bool {
    return payload != nullptr && payload->size() == hdr.payload_len;
  };
  // handle message to ourselves
  switch (hdr.operation) {
    case message_type::server_handshake: {
      node_id source_node;
      basp::routing_table::endpoint autoconn_addr;
      actor_id aid = invalid_actor_id;
      std::set<std::string> sigs;
      if (!payload_valid()) {
        CAF_LOG_ERROR("fail to receive the app identifier");
        return false;
      } else {
        binary_deserializer bd{ctx, *payload};
        std::string remote_appid;
        if (bd(source_node, remote_appid, autoconn_addr))
          return false;
        auto appid = get_or(callee_.config(), "middleman.app-identifier",
                            defaults::middleman::app_identifier);
        if (appid != remote_appid) {
          CAF_LOG_ERROR("app identifier mismatch");
          return false;
        }
        if (bd(aid, sigs))
          return false;
      }
      // Close self connection immediately after handshake.
      if (source_node == this_node_) {
        CAF_LOG_DEBUG("close connection to self immediately");
        callee_.finalize_handshake(source_node, aid, sigs);
        return false;
      }
      auto lr = tbl_.lookup(source_node);
      // Close redundant connections.
      if (lr.hdl) {
        CAF_LOG_DEBUG("close redundant connection:" << CAF_ARG(source_node));
        callee_.finalize_handshake(source_node, aid, sigs);
        return false;
      }
      // Add new route to this node.
      CAF_LOG_DEBUG("new connection:" << CAF_ARG(source_node));
      if (lr.known)
        tbl_.handle(source_node, hdl);
      else
        tbl_.add(source_node, hdl);
      // Store autoconnect address.
      auto peer_server = system().registry().get(atom("PeerServ"));
      anon_send(actor_cast<actor>(peer_server), put_atom::value,
                to_string(source_node), make_message(std::move(autoconn_addr)));
      write_client_handshake(ctx, callee_.get_buffer(hdl));
      flush(hdl);
      callee_.learned_new_node(source_node);
      callee_.finalize_handshake(source_node, aid, sigs);
      callee_.send_buffered_messages(ctx, source_node, hdl);
      break;
    }
    case message_type::client_handshake: {
      if (!payload_valid()) {
        CAF_LOG_ERROR("fail to receive the app identifier");
        return false;
      }
      binary_deserializer bd{ctx, *payload};
      node_id source_node;
      std::string remote_appid;
      basp::routing_table::endpoint autoconn_addr;
      // TODO: Read addrs separately.
      if (bd(source_node, remote_appid, autoconn_addr))
        return false;
      auto appid = get_if<std::string>(&callee_.config(),
                                       "middleman.app-identifier");
      if ((appid && *appid != remote_appid)
          || (!appid && !remote_appid.empty())) {
        CAF_LOG_ERROR("app identifier mismatch");
        return false;
      }
      auto lr = tbl_.lookup(source_node);
      if (lr.hdl) {
        CAF_LOG_DEBUG("received second client handshake:"
                     << CAF_ARG(source_node));
        break;
      }
      // Add this node to our contacts.
      CAF_LOG_DEBUG("new connection:" << CAF_ARG(source_node));
      // Either add a new node or add the handle to a known one.
      if (lr.known)
        tbl_.handle(source_node, hdl);
      else
        tbl_.add(source_node, hdl);
      callee_.learned_new_node(source_node);
      callee_.send_buffered_messages(ctx, source_node, hdl);
      // Store autoconnect address.
      auto peer_server = system().registry().get(atom("PeerServ"));
      anon_send(actor_cast<actor>(peer_server), put_atom::value,
                to_string(source_node), make_message(std::move(autoconn_addr)));
      break;
    }
    case message_type::dispatch_message: {
      // Sanity checks.
      if (!payload_valid())
        return false;
      auto source_node = tbl_.lookup(hdl);
      if (source_node == none) {
        CAF_LOG_ERROR("received dispatch_message before handshake");
        return false;
      }
      // Deserialize payload.
      binary_deserializer bd{ctx, *payload};
      auto receiver_name = static_cast<atom_value>(0);
      std::vector<strong_actor_ptr> forwarding_stack;
      message msg;
      if (hdr.has(header::named_receiver_flag)) {
        if (bd(receiver_name))
          return false;
      }
      if (bd(forwarding_stack, msg))
        return false;
      // Dispatch to callee_.
      CAF_LOG_DEBUG(CAF_ARG(forwarding_stack) << CAF_ARG(msg));
      if (hdr.has(header::named_receiver_flag))
        callee_.deliver(source_node, hdr.source_actor, receiver_name,
                        make_message_id(hdr.operation_data),
                        forwarding_stack, msg);
      else
        callee_.deliver(source_node, hdr.source_actor, hdr.dest_actor,
                        make_message_id(hdr.operation_data),
                        forwarding_stack, msg);
      break;
    }
    case message_type::announce_proxy: {
      auto source_node = tbl_.lookup(hdl);
      if (source_node == none) {
        CAF_LOG_ERROR("received announce_proxy before handshake");
        return false;
      }
      callee_.proxy_announced(source_node, hdr.dest_actor);
      break;
    }
    case message_type::kill_proxy: {
      // Sanity checks.
      if (!payload_valid())
        return false;
      auto source_node = tbl_.lookup(hdl);
      if (source_node == none) {
        CAF_LOG_ERROR("received announce_proxy before handshake");
        return false;
      }
      // Deserialize exit reason.
      binary_deserializer bd{ctx, *payload};
      error fail_state;
      if (bd(fail_state))
        return false;
      // Remove proxy from registry.
      callee_.proxies().erase(source_node, hdr.source_actor,
                              std::move(fail_state));
      break;
    }
    case message_type::heartbeat: {
      auto source_node = tbl_.lookup(hdl);
      if (source_node == none) {
        CAF_LOG_ERROR("received announce_proxy before handshake");
        return false;
      }
      callee_.handle_heartbeat(source_node);
      break;
    }
    default:
      CAF_LOG_ERROR("invalid operation");
      return false;
  }
  return true;
}

} // namespace basp
} // namespace io
} // namespace caf
