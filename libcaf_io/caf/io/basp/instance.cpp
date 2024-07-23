// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/basp/instance.hpp"

#include "caf/io/basp/remote_message_handler.hpp"
#include "caf/io/basp/version.hpp"
#include "caf/io/basp/worker.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/assert.hpp"
#include "caf/log/io.hpp"
#include "caf/settings.hpp"
#include "caf/telemetry/histogram.hpp"
#include "caf/telemetry/timer.hpp"

#include <algorithm>

namespace caf::io::basp {

instance::callee::callee(actor_system& sys, proxy_registry::backend& backend)
  : namespace_(sys, backend) {
  // nop
}

instance::callee::~callee() {
  // nop
}

instance::instance(abstract_broker* parent, callee& lstnr)
  : sys_(&parent->system()),
    tbl_(parent),
    this_node_(parent->system().node()),
    callee_(lstnr) {
  CAF_ASSERT(this_node_ != none);
  size_t workers;
  if (auto workers_cfg = get_as<size_t>(config(), "caf.middleman.workers"))
    workers = *workers_cfg;
  else
    workers = std::min(3u, std::thread::hardware_concurrency() / 4u) + 1;
  for (size_t i = 0; i < workers; ++i)
    hub_.add_new_worker(queue_, proxies());
}

connection_state instance::handle(scheduler* ctx, new_data_msg& dm, header& hdr,
                                  bool is_payload) {
  auto lg = log::io::trace("dm = {}, is_payload = {}", dm, is_payload);
  // function object providing cleanup code on errors
  auto err = [&](connection_state code) {
    if (auto nid = tbl_.erase_direct(dm.handle))
      callee_.purge_state(nid);
    return code;
  };
  byte_buffer* payload = nullptr;
  if (is_payload) {
    payload = &dm.buf;
    if (payload->size() != hdr.payload_len) {
      log::io::warning("received invalid payload, expected {} bytes, got {}",
                       hdr.payload_len, payload->size());
      return err(malformed_message);
    }
  } else {
    binary_deserializer source{*sys_, dm.buf};
    if (!source.apply(hdr)) {
      log::io::warning("failed to receive header: {}", source.get_error());
      return err(malformed_message);
    }
    if (!valid(hdr)) {
      log::io::warning("received invalid header: hdr = {}", hdr);
      return err(malformed_message);
    }
    if (hdr.payload_len > 0) {
      log::io::debug("await payload before processing further");
      return await_payload;
    }
  }
  log::io::debug("hdr = {}", hdr);
  return handle(ctx, dm.handle, hdr, payload);
}

void instance::handle_heartbeat(scheduler* ctx) {
  auto lg = log::io::trace("");
  for (auto& kvp : tbl_.direct_by_hdl_) {
    auto lg = log::io::trace("kvp.first = {}, kvp.second = {}", kvp.first,
                             kvp.second);
    write_heartbeat(ctx, callee_.get_buffer(kvp.first));
    callee_.flush(kvp.first);
  }
}

std::optional<routing_table::route> instance::lookup(const node_id& target) {
  return tbl_.lookup(target);
}

void instance::flush(const routing_table::route& path) {
  callee_.flush(path.hdl);
}

void instance::write(scheduler* ctx, const routing_table::route& r, header& hdr,
                     payload_writer* writer) {
  auto lg = log::io::trace("hdr = {}", hdr);
  CAF_ASSERT(hdr.payload_len == 0 || writer != nullptr);
  write(*sys_, ctx, callee_.get_buffer(r.hdl), hdr, writer);
  flush(r);
}

void instance::add_published_actor(uint16_t port,
                                   strong_actor_ptr published_actor,
                                   std::set<std::string> published_interface) {
  auto lg = log::io::trace(
    "port = {}, published_actor = {}, published_interface = {}", port,
    published_actor, published_interface);
  using std::swap;
  auto& entry = published_actors_[port];
  swap(entry.first, published_actor);
  swap(entry.second, published_interface);
}

size_t instance::remove_published_actor(uint16_t port,
                                        removed_published_actor* cb) {
  auto lg = log::io::trace("port = {}", port);
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
  auto lg = log::io::trace("whom = {}, port = {}", whom, port);
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

bool instance::dispatch(scheduler* ctx, const strong_actor_ptr& sender,
                        const node_id& dest_node, uint64_t dest_actor,
                        uint8_t flags, message_id mid, const message& msg) {
  auto lg = log::io::trace("sender = {}, dest_node = {}, mid = {}, msg = {}",
                           sender, dest_node, mid, msg);
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
      return sink.apply(msg);
    });
    write(*sys_, ctx, callee_.get_buffer(path->hdl), hdr, &writer);
  } else {
    header hdr{message_type::routed_message,
               flags,
               0,
               mid.integer_value(),
               sender ? sender->id() : invalid_actor_id,
               dest_actor};
    auto writer = make_callback([&](binary_serializer& sink) {
      log::io::debug(
        "send routed message: source_node = {} dest_node = {} msg = {}",
        source_node, dest_node, msg);
      return sink.apply(source_node)  //
             && sink.apply(dest_node) //
             && sink.apply(msg);
    });
    write(*sys_, ctx, callee_.get_buffer(path->hdl), hdr, &writer);
  }
  flush(*path);
  return true;
}

void instance::write(actor_system& sys, scheduler*, byte_buffer& buf,
                     header& hdr, payload_writer* pw) {
  auto lg = log::io::trace("hdr = {}", hdr);
  binary_serializer sink{sys, buf};
  if (pw != nullptr) {
    // Write the BASP header after the payload.
    auto header_offset = buf.size();
    sink.skip(header_size);
    auto& mm_metrics = sys.middleman().metric_singletons;
    auto t0 = telemetry::timer::clock_type::now();
    if (!(*pw)(sink)) {
      log::io::error("{}", sink.get_error());
      return;
    }
    telemetry::timer::observe(mm_metrics.serialization_time, t0);
    sink.seek(header_offset);
    auto payload_len = buf.size() - (header_offset + basp::header_size);
    auto signed_payload_len = static_cast<uint32_t>(payload_len);
    mm_metrics.outbound_messages_size->observe(signed_payload_len);
    hdr.payload_len = static_cast<uint32_t>(payload_len);
  }
  if (!sink.apply(hdr))
    log::io::error("{}", sink.get_error());
}

void instance::write_server_handshake(scheduler* ctx, byte_buffer& out_buf,
                                      std::optional<uint16_t> port) {
  auto lg = log::io::trace("port = {}", port);
  using namespace detail;
  published_actor* pa = nullptr;
  if (port) {
    auto i = published_actors_.find(*port);
    if (i != published_actors_.end())
      pa = &i->second;
  }
  if (!pa && port) {
    log::io::debug("no actor published");
  }
  auto writer = make_callback([&](binary_serializer& sink) {
    using string_list = std::vector<std::string>;
    string_list app_ids;
    if (auto ids = get_as<string_list>(config(),
                                       "caf.middleman.app-identifiers"))
      app_ids = std::move(*ids);
    else
      app_ids.emplace_back(std::string{defaults::middleman::app_identifier});
    auto aid = invalid_actor_id;
    auto iface = std::set<std::string>{};
    if (pa != nullptr && pa->first != nullptr) {
      aid = pa->first->id();
      iface = pa->second;
    }
    return sink.apply(this_node_) //
           && sink.apply(app_ids) //
           && sink.apply(aid)     //
           && sink.apply(iface);
  });
  header hdr{message_type::server_handshake,
             0,
             0,
             version,
             invalid_actor_id,
             invalid_actor_id};
  write(*sys_, ctx, out_buf, hdr, &writer);
}

void instance::write_client_handshake(scheduler* ctx, byte_buffer& buf) {
  auto writer = make_callback([&](binary_serializer& sink) { //
    return sink.apply(this_node_);
  });
  header hdr{message_type::client_handshake,
             0,
             0,
             0,
             invalid_actor_id,
             invalid_actor_id};
  write(*sys_, ctx, buf, hdr, &writer);
}

void instance::write_monitor_message(scheduler* ctx, byte_buffer& buf,
                                     const node_id& dest_node, actor_id aid) {
  auto lg = log::io::trace("dest_node = {}, aid = {}", dest_node, aid);
  auto writer = make_callback([&](binary_serializer& sink) { //
    return sink.apply(this_node_) && sink.apply(dest_node);
  });
  header hdr{message_type::monitor_message, 0, 0, 0, invalid_actor_id, aid};
  write(*sys_, ctx, buf, hdr, &writer);
}

void instance::write_down_message(scheduler* ctx, byte_buffer& buf,
                                  const node_id& dest_node, actor_id aid,
                                  const error& rsn) {
  auto lg = log::io::trace("dest_node = {}, aid = {}, rsn = {}", dest_node, aid,
                           rsn);
  auto writer = make_callback([&](binary_serializer& sink) {
    return sink.apply(this_node_) && sink.apply(dest_node) && sink.apply(rsn);
  });
  header hdr{message_type::down_message, 0, 0, 0, aid, invalid_actor_id};
  write(*sys_, ctx, buf, hdr, &writer);
}

void instance::write_heartbeat(scheduler* ctx, byte_buffer& buf) {
  auto lg = log::io::trace("");
  header hdr{message_type::heartbeat, 0, 0, 0, invalid_actor_id,
             invalid_actor_id};
  write(*sys_, ctx, buf, hdr);
}

connection_state instance::handle(scheduler* ctx, connection_handle hdl,
                                  header& hdr, byte_buffer* payload) {
  auto lg = log::io::trace("hdl = {}, hdr = {}", hdl, hdr);
  // Check payload validity.
  if (payload == nullptr) {
    if (hdr.payload_len != 0) {
      log::io::warning("missing payload");
      return malformed_message;
    }
  } else if (hdr.payload_len != payload->size()) {
    log::io::warning("actual payload size differs from advertised size");
    return malformed_message;
  }
  // Dispatch by message type.
  switch (hdr.operation) {
    case message_type::server_handshake: {
      using string_list = std::vector<std::string>;
      // Deserialize payload.
      binary_deserializer source{*sys_, *payload};
      node_id source_node;
      string_list app_ids;
      actor_id aid = invalid_actor_id;
      std::set<std::string> sigs;
      if (!source.apply(source_node) //
          || !source.apply(app_ids)  //
          || !source.apply(aid)      //
          || !source.apply(sigs)) {
        log::io::warning(
          "unable to deserialize payload of server handshake: {}",
          source.get_error());
        return serializing_basp_payload_failed;
      }
      // Check the application ID.
      string_list whitelist;
      if (auto ls = get_as<string_list>(config(),
                                        "caf.middleman.app-identifiers"))
        whitelist = std::move(*ls);
      else
        whitelist.emplace_back(
          std::string{defaults::middleman::app_identifier});
      auto i = std::find_first_of(app_ids.begin(), app_ids.end(),
                                  whitelist.begin(), whitelist.end());
      if (i == app_ids.end()) {
        log::io::warning("refuse to connect to server due to app ID mismatch: "
                         "app_ids = {} whitelist = {}",
                         app_ids, whitelist);
        return incompatible_application_ids;
      }
      // Close connection to ourselves immediately after sending client HS.
      if (source_node == this_node_) {
        log::io::debug("close connection to self immediately");
        callee_.finalize_handshake(source_node, aid, sigs);
        return redundant_connection;
      }
      // Close this connection if we already have a direct connection.
      if (tbl_.lookup_direct(source_node)) {
        log::io::debug("close redundant direct connection: source_node = {}",
                       source_node);
        callee_.finalize_handshake(source_node, aid, sigs);
        return redundant_connection;
      }
      // Add direct route to this node and remove any indirect entry.
      log::io::debug("new direct connection: source_node = {}", source_node);
      tbl_.add_direct(hdl, source_node);
      auto was_indirect = tbl_.erase_indirect(source_node);
      // write handshake as client in response
      auto path = tbl_.lookup(source_node);
      if (!path) {
        log::io::error("no route to host after server handshake");
        return no_route_to_receiving_node;
      }
      callee_.learned_new_node_directly(source_node, was_indirect);
      callee_.finalize_handshake(source_node, aid, sigs);
      break;
    }
    case message_type::client_handshake: {
      // Deserialize payload.
      binary_deserializer source{*sys_, *payload};
      node_id source_node;
      if (!source.apply(source_node)) {
        log::io::warning(
          "unable to deserialize payload of client handshake: {}",
          source.get_error());
        return serializing_basp_payload_failed;
      }
      // Drop repeated handshakes.
      if (tbl_.lookup_direct(source_node)) {
        log::io::debug("received repeated client handshake: source_node = {}",
                       source_node);
        break;
      }
      // Add direct route to this node and remove any indirect entry.
      log::io::debug("new direct connection: source_node = {}", source_node);
      tbl_.add_direct(hdl, source_node);
      auto was_indirect = tbl_.erase_indirect(source_node);
      callee_.learned_new_node_directly(source_node, was_indirect);
      break;
    }
    case message_type::routed_message: {
      // Deserialize payload.
      binary_deserializer source{*sys_, *payload};
      node_id source_node;
      node_id dest_node;
      if (!source.apply(source_node) || !source.apply(dest_node)) {
        log::io::warning("unable to deserialize source and destination for "
                         "routed message: {}",
                         source.get_error());
        return serializing_basp_payload_failed;
      }
      if (dest_node != this_node_) {
        forward(ctx, dest_node, hdr, *payload);
        return await_header;
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
        log::io::debug("launch BASP worker for deserializing a {}",
                       hdr.operation);
        worker->launch(last_hop, hdr, *payload);
      } else {
        log::io::debug("out of BASP workers, continue deserializing a {}",
                       hdr.operation);
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
        f.handle_remote_message(*sys_, callee_.current_scheduler());
      }
      break;
    }
    case message_type::monitor_message: {
      // Deserialize payload.
      binary_deserializer source{*sys_, *payload};
      node_id source_node;
      node_id dest_node;
      if (!source.apply(source_node) || !source.apply(dest_node)) {
        log::io::warning("unable to deserialize payload of monitor message: {}",
                         source.get_error());
        return serializing_basp_payload_failed;
      }
      if (dest_node == this_node_)
        callee_.proxy_announced(source_node, hdr.dest_actor);
      else
        forward(ctx, dest_node, hdr, *payload);
      break;
    }
    case message_type::down_message: {
      // Deserialize payload.
      binary_deserializer source{*sys_, *payload};
      node_id source_node;
      node_id dest_node;
      error fail_state;
      if (!source.apply(source_node)  //
          || !source.apply(dest_node) //
          || !source.apply(fail_state)) {
        log::io::warning("unable to deserialize payload of down message: {}",
                         source.get_error());
        return serializing_basp_payload_failed;
      }
      if (dest_node == this_node_) {
        // Delay this message to make sure we don't skip in-flight messages.
        auto msg_id = queue_.new_id();
        auto ptr = make_mailbox_element(nullptr, make_message_id(),
                                        delete_atom_v, source_node,
                                        hdr.source_actor,
                                        std::move(fail_state));
        queue_.push(callee_.current_scheduler(), msg_id, callee_.this_actor(),
                    std::move(ptr));
      } else {
        forward(ctx, dest_node, hdr, *payload);
      }
      break;
    }
    case message_type::heartbeat: {
      auto lg = log::io::trace("received heartbeat");
      callee_.handle_heartbeat();
      break;
    }
    default: {
      log::io::error("invalid operation");
      return malformed_message;
    }
  }
  return await_header;
}

void instance::forward(scheduler*, const node_id& dest_node, const header& hdr,
                       byte_buffer& payload) {
  auto lg = log::io::trace("dest_node = {}, hdr = {}, payload = {}", dest_node,
                           hdr, payload);
  auto path = lookup(dest_node);
  if (path) {
    binary_serializer sink{*sys_, callee_.get_buffer(path->hdl)};
    if (!sink.apply(hdr)) {
      log::io::error("unable to serialize BASP header: {}", sink.get_error());
      return;
    }
    sink.value(span<const std::byte>{payload.data(), payload.size()});
    flush(*path);
  } else {
    log::io::warning("cannot forward message, no route to destination");
  }
}

} // namespace caf::io::basp
