/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/basp/application.hpp"

#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/byte.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/parse.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/logger.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/no_stages.hpp"
#include "caf/none.hpp"
#include "caf/sec.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/type_erased_tuple.hpp"

namespace caf {
namespace net {
namespace basp {

application::application(proxy_registry& proxies) : proxies_(proxies) {
  // nop
}

expected<std::vector<byte>> application::serialize(actor_system& sys,
                                                   const type_erased_tuple& x) {
  std::vector<byte> result;
  serializer_impl<std::vector<byte>> sink{sys, result};
  if (auto err = message::save(sink, x))
    return err;
  return result;
}

strong_actor_ptr application::resolve_local_path(string_view path) {
  CAF_LOG_TRACE(CAF_ARG(path));
  // We currently support two path formats: `id/<actor_id>` and `name/<atom>`.
  static constexpr string_view id_prefix = "id/";
  if (starts_with(path, id_prefix)) {
    path.remove_prefix(id_prefix.size());
    actor_id aid;
    if (auto err = detail::parse(path, aid))
      return nullptr;
    return system().registry().get(aid);
  }
  static constexpr string_view name_prefix = "name/";
  if (starts_with(path, name_prefix)) {
    path.remove_prefix(name_prefix.size());
    atom_value name;
    if (auto err = detail::parse(path, name))
      return nullptr;
    return system().registry().get(name);
  }
  return nullptr;
}

void application::resolve_remote_path(write_packet_callback& write_packet,
                                      string_view path, actor listener) {
  CAF_LOG_TRACE(CAF_ARG(path) << CAF_ARG(listener));
  buf_.clear();
  serializer_impl<buffer_type> sink{&executor_, buf_};
  if (auto err = sink(path)) {
    CAF_LOG_ERROR("unable to serialize path");
    return;
  }
  auto req_id = next_request_id_++;
  auto hdr = to_bytes(header{message_type::resolve_request,
                             static_cast<uint32_t>(buf_.size()), req_id});
  if (auto err = write_packet(hdr, buf_)) {
    CAF_LOG_ERROR("unable to write resolve_request header");
    return;
  }
  response_promise rp{nullptr, actor_cast<strong_actor_ptr>(listener),
                      no_stages, make_message_id()};
  pending_resolves_.emplace(req_id, std::move(rp));
}

error application::write(write_packet_callback& write_packet,
                         std::unique_ptr<endpoint_manager::message> ptr) {
  CAF_ASSERT(ptr != nullptr);
  CAF_ASSERT(ptr->msg != nullptr);
  buf_.clear();
  serializer_impl<buffer_type> sink{system(), buf_};
  const auto& src = ptr->msg->sender;
  const auto& dst = ptr->receiver;
  if (dst == nullptr) {
    // TODO: valid?
    return none;
  }
  if (src != nullptr) {
    auto src_id = src->id();
    system().registry().put(src_id, src);
    if (auto err = sink(src->node(), src_id, dst->id(), ptr->msg->stages))
      return err;
  } else {
    if (auto err = sink(node_id{}, actor_id{0}, dst->id(), ptr->msg->stages))
      return err;
  }
  // TODO: avoid extra copy of the payload
  buf_.insert(buf_.end(), ptr->payload.begin(), ptr->payload.end());
  header hdr{message_type::actor_message, static_cast<uint32_t>(buf_.size()),
             ptr->msg->mid.integer_value()};
  auto bytes = to_bytes(hdr);
  return write_packet(make_span(bytes), make_span(buf_));
}

error application::handle(size_t& next_read_size,
                          write_packet_callback& write_packet,
                          byte_span bytes) {
  CAF_LOG_TRACE(CAF_ARG(state_) << CAF_ARG2("bytes.size", bytes.size()));
  switch (state_) {
    case connection_state::await_handshake_header: {
      if (bytes.size() != header_size)
        return ec::unexpected_number_of_bytes;
      hdr_ = header::from_bytes(bytes);
      if (hdr_.type != message_type::handshake)
        return ec::missing_handshake;
      if (hdr_.operation_data != version)
        return ec::version_mismatch;
      if (hdr_.payload_len == 0)
        return ec::missing_payload;
      state_ = connection_state::await_handshake_payload;
      next_read_size = hdr_.payload_len;
      return none;
    }
    case connection_state::await_handshake_payload: {
      if (auto err = handle_handshake(write_packet, hdr_, bytes))
        return err;
      state_ = connection_state::await_header;
      return none;
    }
    case connection_state::await_header: {
      if (bytes.size() != header_size)
        return ec::unexpected_number_of_bytes;
      hdr_ = header::from_bytes(bytes);
      if (hdr_.payload_len == 0)
        return handle(write_packet, hdr_, byte_span{});
      next_read_size = hdr_.payload_len;
      state_ = connection_state::await_payload;
      return none;
    }
    case connection_state::await_payload: {
      if (bytes.size() != hdr_.payload_len)
        return ec::unexpected_number_of_bytes;
      state_ = connection_state::await_header;
      return handle(write_packet, hdr_, bytes);
    }
    default:
      return ec::illegal_state;
  }
}

error application::handle(write_packet_callback& write_packet, header hdr,
                          byte_span payload) {
  CAF_LOG_TRACE(CAF_ARG(hdr) << CAF_ARG2("payload.size", payload.size()));
  switch (hdr.type) {
    case message_type::handshake:
      return ec::unexpected_handshake;
    case message_type::actor_message:
      return handle_actor_message(write_packet, hdr, payload);
    case message_type::resolve_request:
      return handle_resolve_request(write_packet, hdr, payload);
    case message_type::resolve_response:
      return handle_resolve_response(write_packet, hdr, payload);
    case message_type::monitor_message:
      return handle_monitor_message(write_packet, hdr, payload);
    case message_type::down_message:
      return handle_down_message(write_packet, hdr, payload);
    case message_type::heartbeat:
      return none;
    default:
      return ec::unimplemented;
  }
}

error application::handle_handshake(write_packet_callback&, header hdr,
                                    byte_span payload) {
  CAF_LOG_TRACE(CAF_ARG(hdr) << CAF_ARG2("payload.size", payload.size()));
  if (hdr.type != message_type::handshake)
    return ec::missing_handshake;
  if (hdr.operation_data != version)
    return ec::version_mismatch;
  node_id peer_id;
  std::vector<std::string> app_ids;
  binary_deserializer source{&executor_, payload};
  if (auto err = source(peer_id, app_ids))
    return err;
  if (!peer_id || app_ids.empty())
    return ec::invalid_handshake;
  auto ids = get_or(system().config(), "middleman.app-identifiers",
                    defaults::middleman::app_identifiers);
  auto predicate = [=](const std::string& x) {
    return std::find(ids.begin(), ids.end(), x) != ids.end();
  };
  if (std::none_of(app_ids.begin(), app_ids.end(), predicate))
    return ec::app_identifiers_mismatch;
  peer_id_ = std::move(peer_id);
  state_ = connection_state::await_header;
  return none;
}

error application::handle_actor_message(write_packet_callback&, header hdr,
                                        byte_span payload) {
  CAF_LOG_TRACE(CAF_ARG(hdr) << CAF_ARG2("payload.size", payload.size()));
  // Deserialize payload.
  actor_id src_id;
  node_id src_node;
  actor_id dst_id;
  std::vector<strong_actor_ptr> fwd_stack;
  message content;
  binary_deserializer source{&executor_, payload};
  if (auto err = source(src_node, src_id, dst_id, fwd_stack, content))
    return err;
  // Sanity checks.
  if (dst_id == 0)
    return ec::invalid_payload;
  // Try to fetch the receiver.
  auto dst_hdl = system().registry().get(dst_id);
  if (dst_hdl == nullptr) {
    CAF_LOG_DEBUG("no actor found for given ID, drop message");
    return caf::none;
  }
  // Try to fetch the sender.
  strong_actor_ptr src_hdl;
  if (src_node != none && src_id != 0)
    src_hdl = proxies_.get_or_put(src_node, src_id);
  // Ship the message.
  auto ptr = make_mailbox_element(std::move(src_hdl),
                                  make_message_id(hdr.operation_data),
                                  std::move(fwd_stack), std::move(content));
  dst_hdl->get()->enqueue(std::move(ptr), nullptr);
  return none;
}

error application::handle_resolve_request(write_packet_callback& write_packet,
                                          header hdr, byte_span payload) {
  CAF_LOG_TRACE(CAF_ARG(hdr) << CAF_ARG2("payload.size", payload.size()));
  CAF_ASSERT(hdr.type == message_type::resolve_request);
  size_t path_size = 0;
  binary_deserializer source{&executor_, payload};
  if (auto err = source.begin_sequence(path_size))
    return err;
  // We expect the payload to consist only of the path.
  if (path_size != source.remaining())
    return ec::invalid_payload;
  auto remainder = source.remainder();
  string_view path{reinterpret_cast<const char*>(remainder.data()),
                   remainder.size()};
  // Write result.
  auto result = resolve_local_path(path);
  buf_.clear();
  actor_id aid;
  std::set<std::string> ifs;
  if (result) {
    aid = result->id();
    system().registry().put(aid, result);
  } else {
    aid = 0;
  }
  // TODO: figure out how to obtain messaging interface.
  serializer_impl<buffer_type> sink{&executor_, buf_};
  if (auto err = sink(aid, ifs))
    return err;
  auto out_hdr = to_bytes(header{message_type::resolve_response,
                                 static_cast<uint32_t>(buf_.size()),
                                 hdr.operation_data});
  return write_packet(out_hdr, buf_);
}

error application::handle_resolve_response(write_packet_callback&, header hdr,
                                           byte_span payload) {
  CAF_LOG_TRACE(CAF_ARG(hdr) << CAF_ARG2("payload.size", payload.size()));
  CAF_ASSERT(hdr.type == message_type::resolve_response);
  auto i = pending_resolves_.find(hdr.operation_data);
  if (i == pending_resolves_.end()) {
    CAF_LOG_ERROR("received unknown ID in resolve_response message");
    return none;
  }
  auto guard = detail::make_scope_guard([&] {
    if (i->second.pending())
      i->second.deliver(sec::remote_lookup_failed);
    pending_resolves_.erase(i);
  });
  actor_id aid;
  std::set<std::string> ifs;
  binary_deserializer source{&executor_, payload};
  if (auto err = source(aid, ifs))
    return err;
  if (aid == 0) {
    i->second.deliver(strong_actor_ptr{nullptr}, std::move(ifs));
    return none;
  }
  i->second.deliver(proxies_.get_or_put(peer_id_, aid), std::move(ifs));
  return none;
}

error application::handle_monitor_message(write_packet_callback& write_packet,
                                          header hdr, byte_span payload) {
  CAF_LOG_TRACE(CAF_ARG(hdr) << CAF_ARG2("payload.size", payload.size()));
  if (!payload.empty())
    return ec::unexpected_payload;
  auto aid = static_cast<actor_id>(hdr.operation_data);
  auto hdl = system().registry().get(aid);
  if (hdl != nullptr) {
    endpoint_manager_ptr mgr = manager_;
    auto nid = peer_id_;
    hdl->get()->attach_functor([mgr, nid, aid](error reason) mutable {
      mgr->enqueue_event(std::move(nid), aid, std::move(reason));
    });
  } else {
    error reason = exit_reason::unknown;
    buf_.clear();
    serializer_impl<buffer_type> sink{&executor_, buf_};
    if (auto err = sink(reason))
      return err;
    auto out_hdr = to_bytes(header{message_type::down_message,
                                   static_cast<uint32_t>(buf_.size()),
                                   hdr.operation_data});
    return write_packet(out_hdr, buf_);
  }
  return none;
}

error application::handle_down_message(write_packet_callback&, header hdr,
                                       byte_span payload) {
  CAF_LOG_TRACE(CAF_ARG(hdr) << CAF_ARG2("payload.size", payload.size()));
  error reason;
  binary_deserializer source{&executor_, payload};
  if (auto err = source(reason))
    return err;
  proxies_.erase(peer_id_, hdr.operation_data, std::move(reason));
  return none;
}

error application::generate_handshake() {
  buf_.clear();
  serializer_impl<buffer_type> sink{&executor_, buf_};
  return sink(system().node(),
              get_or(system().config(), "middleman.app-identifiers",
                     defaults::middleman::app_identifiers));
}

} // namespace basp
} // namespace net
} // namespace caf
