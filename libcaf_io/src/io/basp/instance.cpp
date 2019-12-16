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
#include "caf/io/basp/remote_message_handler.hpp"
#include "caf/io/basp/version.hpp"
#include "caf/io/basp/worker.hpp"
#include "caf/settings.hpp"

namespace caf::io::basp {

instance::callee::callee(actor_system& sys, proxy_registry::backend& backend)
  : namespace_(sys, backend) {
  // nop
}

instance::callee::~callee() {
  // nop
}

instance::instance(abstract_broker* parent, callee& lstnr)
  : tbl_(parent), this_node_(parent->system().node()), callee_(lstnr) {
  CAF_ASSERT(this_node_ != none);
  auto workers
    = get_or(config(), "middleman.workers", defaults::middleman::workers);
  for (size_t i = 0; i < workers; ++i)
    hub_.add_new_worker(queue_, proxies());
}

connection_state instance::handle(execution_unit* ctx, new_data_msg& dm,
                                  header& hdr, bool is_payload) {
  CAF_LOG_TRACE(CAF_ARG(dm) << CAF_ARG(is_payload));
  // function object providing cleanup code on errors
  auto err = [&]() -> connection_state {
    if (auto nid = tbl_.erase_direct(dm.handle))
      callee_.purge_state(nid);
    return close_connection;
  };
  byte_buffer* payload = nullptr;
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
  for (auto& kvp : tbl_.direct_by_hdl_) {
    CAF_LOG_TRACE(CAF_ARG(kvp.first) << CAF_ARG(kvp.second));
    write_heartbeat(ctx, callee_.get_buffer(kvp.first));
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
  CAF_LOG_TRACE(CAF_ARG(port)
                << CAF_ARG(published_actor) << CAF_ARG(published_interface));
  using std::swap;
  auto& entry = published_actors_[port];
  swap(entry.first, published_actor);
  swap(entry.second, published_interface);
}

size_t
instance::remove_published_actor(uint16_t port, removed_published_actor* cb) {
  CAF_LOG_TRACE(CAF_ARG(port));
  auto i = published_actors_.find(port);
  if (i == published_actors_.end())
    return 0;
  if (cb != nullptr)
    (*cb)(i->second.first, i->first);
  published_actors_.erase(i);
  return 1;
}

size_t instance::remove_published_actor(const actor_addr& whom, uint16_t port,
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
                        const node_id& dest_node, uint64_t dest_actor,
                        uint8_t flags, message_id mid, const message& msg) {
  CAF_LOG_TRACE(CAF_ARG(sender)
                << CAF_ARG(dest_node) << CAF_ARG(mid) << CAF_ARG(msg));
  CAF_ASSERT(dest_node && this_node_ != dest_node);
  auto path = lookup(dest_node);
  if (!path)
    return false;
  auto& source_node = sender ? sender->node() : this_node_;
  if (dest_node == path->next_hop && source_node == this_node_) {
    header hdr{message_type::direct_message,
               flags,
               0,
               mid.integer_value(),
               sender ? sender->id() : invalid_actor_id,
               dest_actor};
    auto writer = make_callback([&](binary_serializer& sink) { //
      return sink(forwarding_stack, msg);
    });
    write(ctx, callee_.get_buffer(path->hdl), hdr, &writer);
  } else {
    header hdr{message_type::routed_message,
               flags,
               0,
               mid.integer_value(),
               sender ? sender->id() : invalid_actor_id,
               dest_actor};
    auto writer = make_callback([&](binary_serializer& sink) {
      return sink(source_node, dest_node, forwarding_stack, msg);
    });
    write(ctx, callee_.get_buffer(path->hdl), hdr, &writer);
  }
  flush(*path);
  return true;
}

void instance::write(execution_unit* ctx, byte_buffer& buf, header& hdr,
                     payload_writer* pw) {
  CAF_LOG_TRACE(CAF_ARG(hdr));
  binary_serializer sink{ctx, buf};
  if (pw != nullptr) {
    // Write the BASP header after the payload.
    auto header_offset = buf.size();
    sink.skip(header_size);
    if (auto err = (*pw)(sink))
      CAF_LOG_ERROR(CAF_ARG(err));
    sink.seek(header_offset);
    auto payload_len = buf.size() - (header_offset + basp::header_size);
    hdr.payload_len = static_cast<uint32_t>(payload_len);
  }
  if (auto err = sink(hdr))
    CAF_LOG_ERROR(CAF_ARG(err));
}

void instance::write_server_handshake(execution_unit* ctx, byte_buffer& out_buf,
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
  auto writer = make_callback([&](binary_serializer& sink) {
    auto app_ids = get_or(config(), "middleman.app-identifiers",
                          defaults::middleman::app_identifiers);
    auto aid = invalid_actor_id;
    auto iface = std::set<std::string>{};
    if (pa != nullptr && pa->first != nullptr) {
      aid = pa->first->id();
      iface = pa->second;
    }
    return sink(this_node_, app_ids, aid, iface);
  });
  header hdr{message_type::server_handshake,
             0,
             0,
             version,
             invalid_actor_id,
             invalid_actor_id};
  write(ctx, out_buf, hdr, &writer);
}

void instance::write_client_handshake(execution_unit* ctx, byte_buffer& buf) {
  auto writer = make_callback([&](binary_serializer& sink) { //
    return sink(this_node_);
  });
  header hdr{message_type::client_handshake,
             0,
             0,
             0,
             invalid_actor_id,
             invalid_actor_id};
  write(ctx, buf, hdr, &writer);
}

void instance::write_monitor_message(execution_unit* ctx, byte_buffer& buf,
                                     const node_id& dest_node, actor_id aid) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(aid));
  auto writer = make_callback([&](binary_serializer& sink) { //
    return sink(this_node_, dest_node);
  });
  header hdr{message_type::monitor_message, 0, 0, 0, invalid_actor_id, aid};
  write(ctx, buf, hdr, &writer);
}

void instance::write_down_message(execution_unit* ctx, byte_buffer& buf,
                                  const node_id& dest_node, actor_id aid,
                                  const error& rsn) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(aid) << CAF_ARG(rsn));
  auto writer = make_callback([&](binary_serializer& sink) { //
    return sink(this_node_, dest_node, rsn);
  });
  header hdr{message_type::down_message, 0, 0, 0, aid, invalid_actor_id};
  write(ctx, buf, hdr, &writer);
}

void instance::write_heartbeat(execution_unit* ctx, byte_buffer& buf) {
  CAF_LOG_TRACE("");
  header hdr{message_type::heartbeat, 0, 0, 0, invalid_actor_id,
             invalid_actor_id};
  write(ctx, buf, hdr);
}

bool instance::handle(execution_unit* ctx, connection_handle hdl, header& hdr,
                      byte_buffer* payload) {
  CAF_LOG_TRACE(CAF_ARG(hdl) << CAF_ARG(hdr));
  // Check payload validity.
  if (payload == nullptr) {
    if (hdr.payload_len != 0) {
      CAF_LOG_WARNING("invalid payload");
      return false;
    }
  } else if (hdr.payload_len != payload->size()) {
    CAF_LOG_WARNING("invalid payload");
    return false;
  }
  // Dispatch by message type.
  switch (hdr.operation) {
    case message_type::server_handshake: {
      // Deserialize payload.
      binary_deserializer bd{ctx, *payload};
      node_id source_node;
      std::vector<std::string> app_ids;
      actor_id aid = invalid_actor_id;
      std::set<std::string> sigs;
      if (auto err = bd(source_node, app_ids, aid, sigs)) {
        CAF_LOG_WARNING("unable to deserialize payload of server handshake:"
                        << ctx->system().render(err));
        return false;
      }
      // Check the application ID.
      auto whitelist = get_or(config(), "middleman.app-identifiers",
                              defaults::middleman::app_identifiers);
      auto i = std::find_first_of(app_ids.begin(), app_ids.end(),
                                  whitelist.begin(), whitelist.end());
      if (i == app_ids.end()) {
        CAF_LOG_WARNING("refuse to connect to server due to app ID mismatch:"
                        << CAF_ARG(app_ids) << CAF_ARG(whitelist));
        return false;
      }
      // Close connection to ourselves immediately after sending client HS.
      if (source_node == this_node_) {
        CAF_LOG_DEBUG("close connection to self immediately");
        callee_.finalize_handshake(source_node, aid, sigs);
        return false;
      }
      // Close this connection if we already have a direct connection.
      if (tbl_.lookup_direct(source_node)) {
        CAF_LOG_DEBUG(
          "close redundant direct connection:" << CAF_ARG(source_node));
        callee_.finalize_handshake(source_node, aid, sigs);
        return false;
      }
      // Add direct route to this node and remove any indirect entry.
      CAF_LOG_DEBUG("new direct connection:" << CAF_ARG(source_node));
      tbl_.add_direct(hdl, source_node);
      auto was_indirect = tbl_.erase_indirect(source_node);
      // write handshake as client in response
      auto path = tbl_.lookup(source_node);
      if (!path) {
        CAF_LOG_ERROR("no route to host after server handshake");
        return false;
      }
      write_client_handshake(ctx, callee_.get_buffer(path->hdl));
      callee_.learned_new_node_directly(source_node, was_indirect);
      callee_.finalize_handshake(source_node, aid, sigs);
      flush(*path);
      break;
    }
    case message_type::client_handshake: {
      // Deserialize payload.
      binary_deserializer bd{ctx, *payload};
      node_id source_node;
      if (auto err = bd(source_node)) {
        CAF_LOG_WARNING("unable to deserialize payload of client handshake:"
                        << ctx->system().render(err));
        return false;
      }
      // Drop repeated handshakes.
      if (tbl_.lookup_direct(source_node)) {
        CAF_LOG_DEBUG(
          "received repeated client handshake:" << CAF_ARG(source_node));
        break;
      }
      // Add direct route to this node and remove any indirect entry.
      CAF_LOG_DEBUG("new direct connection:" << CAF_ARG(source_node));
      tbl_.add_direct(hdl, source_node);
      auto was_indirect = tbl_.erase_indirect(source_node);
      callee_.learned_new_node_directly(source_node, was_indirect);
      break;
    }
    case message_type::routed_message: {
      // Deserialize payload.
      binary_deserializer bd{ctx, *payload};
      node_id source_node;
      node_id dest_node;
      if (auto err = bd(source_node, dest_node)) {
        CAF_LOG_WARNING(
          "unable to deserialize source and destination for routed message:"
          << ctx->system().render(err));
        return false;
      }
      if (dest_node != this_node_) {
        forward(ctx, dest_node, hdr, *payload);
        return true;
      }
      auto last_hop = tbl_.lookup_direct(hdl);
      if (source_node != none && source_node != this_node_
          && last_hop != source_node
          && tbl_.add_indirect(last_hop, source_node))
        callee_.learned_new_node_indirectly(source_node);
    }
    // fall through
    case message_type::direct_message: {
      auto worker = hub_.pop();
      auto last_hop = tbl_.lookup_direct(hdl);
      if (worker != nullptr) {
        CAF_LOG_DEBUG("launch BASP worker for deserializing a"
                      << hdr.operation);
        worker->launch(last_hop, hdr, *payload);
      } else {
        CAF_LOG_DEBUG("out of BASP workers, continue deserializing a"
                      << hdr.operation);
        // If no worker is available then we have no other choice than to take
        // the performance hit and deserialize in this thread.
        struct handler : remote_message_handler<handler> {
          handler(message_queue* queue, proxy_registry* proxies,
                  actor_system* system, node_id last_hop, basp::header& hdr,
                  byte_buffer& payload)
            : queue_(queue),
              proxies_(proxies),
              system_(system),
              last_hop_(std::move(last_hop)),
              hdr_(hdr),
              payload_(payload) {
            msg_id_ = queue_->new_id();
          }
          message_queue* queue_;
          proxy_registry* proxies_;
          actor_system* system_;
          node_id last_hop_;
          basp::header& hdr_;
          byte_buffer& payload_;
          uint64_t msg_id_;
        };
        handler f{&queue_, &proxies(), &system(), last_hop, hdr, *payload};
        f.handle_remote_message(callee_.current_execution_unit());
      }
      break;
    }
    case message_type::monitor_message: {
      // Deserialize payload.
      binary_deserializer bd{ctx, *payload};
      node_id source_node;
      node_id dest_node;
      if (auto err = bd(source_node, dest_node)) {
        CAF_LOG_WARNING("unable to deserialize payload of monitor message:"
                        << ctx->system().render(err));
        return false;
      }
      if (dest_node == this_node_)
        callee_.proxy_announced(source_node, hdr.dest_actor);
      else
        forward(ctx, dest_node, hdr, *payload);
      break;
    }
    case message_type::down_message: {
      // Deserialize payload.
      binary_deserializer bd{ctx, *payload};
      node_id source_node;
      node_id dest_node;
      error fail_state;
      if (auto err = bd(source_node, dest_node, fail_state)) {
        CAF_LOG_WARNING("unable to deserialize payload of down message:"
                        << ctx->system().render(err));
        return false;
      }
      if (dest_node == this_node_) {
        // Delay this message to make sure we don't skip in-flight messages.
        auto msg_id = queue_.new_id();
        auto ptr = make_mailbox_element(nullptr, make_message_id(), {},
                                        delete_atom_v, source_node,
                                        hdr.source_actor,
                                        std::move(fail_state));
        queue_.push(callee_.current_execution_unit(), msg_id,
                    callee_.this_actor(), std::move(ptr));
      } else {
        forward(ctx, dest_node, hdr, *payload);
      }
      break;
    }
    case message_type::heartbeat: {
      CAF_LOG_TRACE("received heartbeat");
      callee_.handle_heartbeat();
      break;
    }
    default: {
      CAF_LOG_ERROR("invalid operation");
      return false;
    }
  }
  return true;
}

void instance::forward(execution_unit* ctx, const node_id& dest_node,
                       const header& hdr, byte_buffer& payload) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(hdr) << CAF_ARG(payload));
  auto path = lookup(dest_node);
  if (path) {
    binary_serializer bs{ctx, callee_.get_buffer(path->hdl)};
    if (auto err = bs(hdr)) {
      CAF_LOG_ERROR("unable to serialize BASP header");
      return;
    }
    bs.apply(span<const byte>{payload.data(), payload.size()});
    flush(*path);
  } else {
    CAF_LOG_WARNING("cannot forward message, no route to destination");
  }
}

} // namespace caf::io::basp
