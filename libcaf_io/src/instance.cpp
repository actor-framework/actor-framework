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
    tbl_.erase_direct(dm.handle, cb);
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
  // needs forwarding?
  if (!is_handshake(hdr) && !is_heartbeat(hdr) && hdr.dest_node != this_node_) {
    CAF_LOG_DEBUG("forward message");
    auto path = lookup(hdr.dest_node);
    if (path) {
      binary_serializer bs{ctx, callee_.get_buffer(path->hdl)};
      auto e = bs(hdr);
      if (e)
        return err();
      if (payload != nullptr)
        bs.apply_raw(payload->size(), payload->data());
      flush(*path);
      notify<hook::message_forwarded>(hdr, payload);
    } else {
      CAF_LOG_INFO("cannot forward message, no route to destination");
      if (hdr.source_node != this_node_) {
        // TODO: signalize error back to sending node
        auto reverse_path = lookup(hdr.source_node);
        if (!reverse_path) {
          CAF_LOG_WARNING("cannot send error message: no route to source");
        } else {
          CAF_LOG_WARNING("not implemented yet: signalize forward failure");
        }
      } else {
        CAF_LOG_WARNING("lost packet with probably spoofed source");
      }
      notify<hook::message_forwarding_failed>(hdr, payload);
    }
    return await_header;
  }
  if (!handle(ctx, dm.handle, hdr, payload))
    return err();
  return await_header;
}

void instance::handle_heartbeat(execution_unit* ctx) {
  CAF_LOG_TRACE("");
  for (auto& kvp: tbl_.direct_by_hdl_) {
    CAF_LOG_TRACE(CAF_ARG(kvp.first) << CAF_ARG(kvp.second));
    write_heartbeat(ctx, callee_.get_buffer(kvp.first), kvp.second);
    callee_.flush(kvp.first);
  }
}

optional<routing_table::route> instance::lookup(const node_id& target) {
  return tbl_.lookup(target);
}

void instance::flush(const routing_table::route& path) {
  callee_.flush(path.hdl);
}

void instance::write(execution_unit* ctx, const routing_table::route& r,
                     header& hdr, payload_writer* writer) {
  CAF_LOG_TRACE(CAF_ARG(hdr));
  CAF_ASSERT(hdr.payload_len == 0 || writer != nullptr);
  write(ctx, callee_.get_buffer(r.hdl), hdr, writer);
  flush(r);
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
  auto path = lookup(receiver->node());
  if (!path) {
    notify<hook::message_sending_failed>(sender, receiver, mid, msg);
    return false;
  }
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<std::vector<strong_actor_ptr>&>(forwarding_stack),
                const_cast<message&>(msg));
  });
  header hdr{message_type::dispatch_message, 0, 0, mid.integer_value(),
             sender ? sender->node() : this_node(), receiver->node(),
             sender ? sender->id() : invalid_actor_id, receiver->id()};
  write(ctx, callee_.get_buffer(path->hdl), hdr, &writer);
  flush(*path);
  notify<hook::message_sent>(sender, path->next_hop, receiver, mid, msg);
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
    auto id = get_or(callee_.config(), "middleman.app-identifier",
                     defaults::middleman::app_identifier);
    auto e = sink(id);
    if (e)
      return e;
    if (pa != nullptr) {
      auto i = pa->first ? pa->first->id() : invalid_actor_id;
      return sink(i, pa->second);
    }
    auto aid = invalid_actor_id;
    std::set<std::string> tmp;
    return sink(aid, tmp);
  });
  header hdr{message_type::server_handshake, 0, 0, version,
             this_node_, none,
             (pa != nullptr) && pa->first ? pa->first->id() : invalid_actor_id,
             invalid_actor_id};
  write(ctx, out_buf, hdr, &writer);
}

void instance::write_client_handshake(execution_unit* ctx,
                                      buffer_type& buf,
                                      const node_id& remote_side,
                                      const node_id& this_node,
                                      const std::string& app_identifier) {
  CAF_LOG_TRACE(CAF_ARG(remote_side));
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<std::string&>(app_identifier));
  });
  header hdr{message_type::client_handshake, 0, 0, 0,
             this_node, remote_side, invalid_actor_id, invalid_actor_id};
  write(ctx, buf, hdr, &writer);
}

void instance::write_client_handshake(execution_unit* ctx,
                                      buffer_type& buf,
                                      const node_id& remote_side) {
  write_client_handshake(ctx, buf, remote_side, this_node_,
                         get_or(callee_.config(), "middleman.app-identifier",
                                defaults::middleman::app_identifier));
}

void instance::write_announce_proxy(execution_unit* ctx, buffer_type& buf,
                                    const node_id& dest_node, actor_id aid) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(aid));
  header hdr{message_type::announce_proxy, 0, 0, 0,
             this_node_, dest_node, invalid_actor_id, aid};
  write(ctx, buf, hdr);
}

void instance::write_kill_proxy(execution_unit* ctx, buffer_type& buf,
                                const node_id& dest_node, actor_id aid,
                                const error& rsn) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(aid) << CAF_ARG(rsn));
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<error&>(rsn));
  });
  header hdr{message_type::kill_proxy, 0, 0, 0,
             this_node_, dest_node, aid, invalid_actor_id};
  write(ctx, buf, hdr, &writer);
}

void instance::write_heartbeat(execution_unit* ctx,
                               buffer_type& buf,
                               const node_id& remote_side) {
  CAF_LOG_TRACE(CAF_ARG(remote_side));
  header hdr{message_type::heartbeat, 0, 0, 0,
             this_node_, remote_side, invalid_actor_id, invalid_actor_id};
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
      actor_id aid = invalid_actor_id;
      std::set<std::string> sigs;
      if (!payload_valid()) {
        CAF_LOG_ERROR("fail to receive the app identifier");
        return false;
      } else {
        binary_deserializer bd{ctx, *payload};
        std::string remote_appid;
        auto e = bd(remote_appid);
        if (e)
          return false;
        auto appid = get_if<std::string>(&callee_.config(),
                                         "middleman.app-identifier");
        if ((appid && *appid != remote_appid)
            || (!appid && !remote_appid.empty())) {
          CAF_LOG_ERROR("app identifier mismatch");
          return false;
        }
        e = bd(aid, sigs);
        if (e)
          return false;
      }
      // close self connection after handshake is done
      if (hdr.source_node == this_node_) {
        CAF_LOG_DEBUG("close connection to self immediately");
        callee_.finalize_handshake(hdr.source_node, aid, sigs);
        return false;
      }
      // close this connection if we already have a direct connection
      if (tbl_.lookup_direct(hdr.source_node)) {
        CAF_LOG_DEBUG("close connection since we already have a "
                      "direct connection: " << CAF_ARG(hdr.source_node));
        callee_.finalize_handshake(hdr.source_node, aid, sigs);
        return false;
      }
      // add direct route to this node and remove any indirect entry
      CAF_LOG_DEBUG("new direct connection:" << CAF_ARG(hdr.source_node));
      tbl_.add_direct(hdl, hdr.source_node);
      auto was_indirect = tbl_.erase_indirect(hdr.source_node);
      // write handshake as client in response
      auto path = tbl_.lookup(hdr.source_node);
      if (!path) {
        CAF_LOG_ERROR("no route to host after server handshake");
        return false;
      }
      write_client_handshake(ctx, callee_.get_buffer(path->hdl),
                             hdr.source_node);
      callee_.learned_new_node_directly(hdr.source_node, was_indirect);
      callee_.finalize_handshake(hdr.source_node, aid, sigs);
      flush(*path);
      break;
    }
    case message_type::client_handshake: {
      if (!payload_valid()) {
        CAF_LOG_ERROR("fail to receive the app identifier");
        return false;
      } else {
        binary_deserializer bd{ctx, *payload};
        std::string remote_appid;
        auto e = bd(remote_appid);
        if (e)
          return false;
        auto appid = get_if<std::string>(&callee_.config(),
                                         "middleman.app-identifier");
        if ((appid && *appid != remote_appid)
            || (!appid && !remote_appid.empty())) {
          CAF_LOG_ERROR("app identifier mismatch");
          return false;
        }
      }
      if (tbl_.lookup_direct(hdr.source_node)) {
        CAF_LOG_DEBUG("received second client handshake:"
                     << CAF_ARG(hdr.source_node));
        break;
      }
      // add direct route to this node and remove any indirect entry
      CAF_LOG_DEBUG("new direct connection:" << CAF_ARG(hdr.source_node));
      tbl_.add_direct(hdl, hdr.source_node);
      auto was_indirect = tbl_.erase_indirect(hdr.source_node);
      callee_.learned_new_node_directly(hdr.source_node, was_indirect);
      break;
    }
    case message_type::dispatch_message: {
      if (!payload_valid())
        return false;
      // in case the sender of this message was received via a third node,
      // we assume that that node to offers a route to the original source
      auto last_hop = tbl_.lookup_direct(hdl);
      if (hdr.source_node != none
          && hdr.source_node != this_node_
          && last_hop != hdr.source_node
          && !tbl_.lookup_direct(hdr.source_node)
          && tbl_.add_indirect(last_hop, hdr.source_node))
        callee_.learned_new_node_indirectly(hdr.source_node);
      binary_deserializer bd{ctx, *payload};
      auto receiver_name = static_cast<atom_value>(0);
      std::vector<strong_actor_ptr> forwarding_stack;
      message msg;
      if (hdr.has(header::named_receiver_flag)) {
        auto e = bd(receiver_name);
        if (e)
          return false;
      }
      auto e = bd(forwarding_stack, msg);
      if (e)
        return false;
      CAF_LOG_DEBUG(CAF_ARG(forwarding_stack) << CAF_ARG(msg));
      if (hdr.has(header::named_receiver_flag))
        callee_.deliver(hdr.source_node, hdr.source_actor, receiver_name,
                        make_message_id(hdr.operation_data),
                        forwarding_stack, msg);
      else
        callee_.deliver(hdr.source_node, hdr.source_actor, hdr.dest_actor,
                        make_message_id(hdr.operation_data),
                        forwarding_stack, msg);
      break;
    }
    case message_type::announce_proxy:
      callee_.proxy_announced(hdr.source_node, hdr.dest_actor);
      break;
    case message_type::kill_proxy: {
      if (!payload_valid())
        return false;
      binary_deserializer bd{ctx, *payload};
      error fail_state;
      auto e = bd(fail_state);
      if (e)
        return false;
      callee_.proxies().erase(hdr.source_node, hdr.source_actor,
                              std::move(fail_state));
      break;
    }
    case message_type::heartbeat: {
      CAF_LOG_TRACE("received heartbeat: " << CAF_ARG(hdr.source_node));
      callee_.handle_heartbeat(hdr.source_node);
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
