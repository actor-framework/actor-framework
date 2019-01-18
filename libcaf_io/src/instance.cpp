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

namespace {

struct seq_num_visitor {
  using result_type = uint16_t;
  seq_num_visitor(instance::callee& c) : cal(c) { }
  template <class T>
  result_type operator()(const T& hdl) {
    return cal.next_sequence_number(hdl);
  }
  instance::callee& cal;
};

} // namespace <anonymous>

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

connection_state instance::handle(execution_unit* ctx, new_data_msg& dm,
                                  header& hdr, bool is_payload) {
  CAF_LOG_TRACE(CAF_ARG(dm) << CAF_ARG(is_payload));
  // Function object providing cleanup code on errors.
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
  // Needs forwarding?
  if (!is_handshake(hdr) && !is_heartbeat(hdr) && hdr.dest_node != this_node_) {
    // TODO: Forwarding should no longer happen.
    return err();
  }
  if (!handle(ctx, dm.handle, hdr, payload, true, none, none))
    return err();
  return await_header;
}

bool instance::handle(execution_unit* ctx, new_datagram_msg& dm,
                      endpoint_context& ep) {
  using itr_t = network::receive_buffer::iterator;
  // Function object providing cleanup code on errors.
  auto err = [&]() -> bool {
    auto cb = make_callback([&](const node_id& nid) -> error {
      callee_.purge_state(nid);
      return none;
    });
    tbl_.erase(dm.handle, cb);
    return false;
  };
  // Extract payload.
  std::vector<char> pl_buf{std::move_iterator<itr_t>(std::begin(dm.buf) +
                                                     basp::header_size),
                           std::move_iterator<itr_t>(std::end(dm.buf))};
  // Resize header.
  dm.buf.resize(basp::header_size);
  // Extract header.
  binary_deserializer bd{ctx, dm.buf};
  auto e = bd(ep.hdr);
  if (e || !valid(ep.hdr)) {
    CAF_LOG_WARNING("received invalid header:" << CAF_ARG(ep.hdr));
    return err();
  }
  CAF_LOG_DEBUG(CAF_ARG(ep.hdr));
  std::vector<char>* payload = nullptr;
  if (ep.hdr.payload_len > 0) {
    payload = &pl_buf;
    if (payload->size() != ep.hdr.payload_len) {
      CAF_LOG_WARNING("received invalid payload");
      return err();
    }
  }
  // Handle ordering of datagrams.
  if (is_greater(ep.hdr.sequence_number, ep.seq_incoming)) {
    // Add early messages to the pending message buffer.
    auto s = ep.hdr.sequence_number;
    callee_.add_pending(ctx, ep, s, std::move(ep.hdr), std::move(pl_buf));
    return true;
  } else if (ep.hdr.sequence_number != ep.seq_incoming) {
    // Drop messages that arrive late.
    CAF_LOG_DEBUG("dropping message " << CAF_ARG(dm));
    return true;
  }
  // This is the expected message.
  ep.seq_incoming += 1;
  // TODO: Add optional reliability here.
  if (!is_handshake(ep.hdr) && !is_heartbeat(ep.hdr)
       && ep.hdr.dest_node != this_node_) {
    // TODO: Forwarding should no longer happen.
    return err();
  }
  if (!handle(ctx, dm.handle, ep.hdr, payload, false, ep, ep.local_port))
    return err();
  // See if the next message was delivered early and is already bufferd.
  if (!callee_.deliver_pending(ctx, ep, false))
    return err();
  return true;
}

void instance::handle_heartbeat(execution_unit* ctx) {
  CAF_LOG_TRACE("");
  for (auto& kvp: tbl_.nid_by_hdl_) {
    CAF_LOG_TRACE(CAF_ARG(kvp.first) << CAF_ARG(kvp.second));
    write_heartbeat(ctx, callee_.get_buffer(kvp.first),
                    kvp.second, visit(seq_num_visitor{callee_}, kvp.first));
    callee_.flush(kvp.first);
  }
}

void instance::flush(endpoint_handle hdl) {
  callee_.flush(hdl);
}

void instance::write(execution_unit* ctx, instance::endpoint_handle hdl,
                     header& hdr, payload_writer* writer) {
  CAF_LOG_TRACE(CAF_ARG(hdr));
  CAF_ASSERT(hdr.payload_len == 0 || writer != nullptr);
  write(ctx, callee_.get_buffer(hdl), hdr, writer);
  callee_.flush(hdl);
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

bool instance::is_greater(sequence_type lhs, sequence_type rhs,
                          sequence_type max_distance) {
  // Distance between lhs and rhs is smaller than max_distance.
  return ((lhs > rhs) && (lhs - rhs <= max_distance)) ||
         ((lhs < rhs) && (rhs - lhs > max_distance));
}

bool instance::dispatch(execution_unit* ctx, const strong_actor_ptr& sender,
                        const std::vector<strong_actor_ptr>& forwarding_stack,
                        const strong_actor_ptr& receiver, message_id mid,
                        const message& msg) {
  CAF_LOG_TRACE(CAF_ARG(sender) << CAF_ARG(receiver)
                << CAF_ARG(mid) << CAF_ARG(msg));
  CAF_ASSERT(receiver && system().node() != receiver->node());
  auto res = tbl_.lookup(receiver->node());
  if (!res.known) {
    notify<hook::message_sending_failed>(sender, receiver, mid, msg);
    return false;
  }
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<std::vector<strong_actor_ptr>&>(forwarding_stack),
                const_cast<message&>(msg));
  });
  header hdr{message_type::dispatch_message, 0, 0, mid.integer_value(),
             sender ? sender->node() : this_node(), receiver->node(),
             sender ? sender->id() : invalid_actor_id, receiver->id(),
             0};
  if (res.hdl) {
    auto hdl = std::move(*res.hdl);
    hdr.sequence_number = visit(seq_num_visitor{callee_}, hdl);
    write(ctx, callee_.get_buffer(hdl), hdr, &writer);
    callee_.flush(hdl);
    notify<hook::message_sent>(sender, receiver->node(), receiver, mid, msg);
    return true;
  } else {
    write(ctx, callee_.get_buffer(receiver->node()), hdr, &writer);
    // TODO: Should the hook really be called here, or should we delay this
    // until communication is established?
    notify<hook::message_sent>(sender, receiver->node(), receiver, mid, msg);
    return true;
  }
}

void instance::write(execution_unit* ctx, buffer_type& buf,
                     header& hdr, payload_writer* pw) {
  CAF_LOG_TRACE(CAF_ARG(hdr));
  error err;
  if (pw != nullptr) {
    auto pos = buf.size();
    // Write payload first (skip first 72 bytes and write header later).
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

void instance::write_server_handshake(execution_unit* ctx,
                                      buffer_type& out_buf,
                                      optional<uint16_t> port,
                                      uint16_t sequence_number) {
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
      return sink(i, pa->second, tbl_.local_addresses());
    }
    auto aid = invalid_actor_id;
    std::set<std::string> tmp;
    return sink(aid, tmp, tbl_.local_addresses());
  });
  header hdr{message_type::server_handshake, 0, 0, version,
             this_node_, none,
             (pa != nullptr) && pa->first ? pa->first->id() : invalid_actor_id,
             invalid_actor_id, sequence_number};
  write(ctx, out_buf, hdr, &writer);
}

void instance::write_client_handshake(execution_unit* ctx,
                                      buffer_type& buf,
                                      const node_id& remote_side,
                                      const node_id& this_node,
                                      const std::string& app_identifier,
                                      uint16_t sequence_number) {
  CAF_LOG_TRACE(CAF_ARG(remote_side));
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<std::string&>(app_identifier),
                tbl_.local_addresses());
  });
  header hdr{message_type::client_handshake, 0, 0, 0,
             this_node, remote_side, invalid_actor_id, invalid_actor_id,
             sequence_number};
  write(ctx, buf, hdr, &writer);
}

void instance::write_client_handshake(execution_unit* ctx,
                                      buffer_type& buf,
                                      const node_id& remote_side,
                                      uint16_t sequence_number) {
  write_client_handshake(ctx, buf, remote_side, this_node_,
                         get_or(callee_.config(), "middleman.app-identifier",
                                defaults::middleman::app_identifier),
                         sequence_number);
}


void instance::write_acknowledge_handshake(execution_unit* ctx,
                                           buffer_type& buf,
                                           const node_id& remote_side,
                                           uint16_t sequence_number) {
  header hdr{message_type::acknowledge_handshake, 0, 0, 0,
             this_node_, remote_side, invalid_actor_id, invalid_actor_id,
             sequence_number};
  write(ctx, buf, hdr);
}

void instance::write_announce_proxy(execution_unit* ctx, buffer_type& buf,
                                    const node_id& dest_node, actor_id aid,
                                    uint16_t sequence_number) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(aid));
  header hdr{message_type::announce_proxy, 0, 0, 0,
             this_node_, dest_node, invalid_actor_id, aid,
             sequence_number};
  write(ctx, buf, hdr);
}

void instance::write_kill_proxy(execution_unit* ctx, buffer_type& buf,
                                const node_id& dest_node, actor_id aid,
                                const error& rsn, uint16_t sequence_number) {
  CAF_LOG_TRACE(CAF_ARG(dest_node) << CAF_ARG(aid) << CAF_ARG(rsn));
  auto writer = make_callback([&](serializer& sink) -> error {
    return sink(const_cast<error&>(rsn));
  });
  header hdr{message_type::kill_proxy, 0, 0, 0,
             this_node_, dest_node, aid, invalid_actor_id,
             sequence_number};
  write(ctx, buf, hdr, &writer);
}

void instance::write_heartbeat(execution_unit* ctx,
                               buffer_type& buf,
                               const node_id& remote_side,
                               uint16_t sequence_number) {
  CAF_LOG_TRACE(CAF_ARG(remote_side));
  header hdr{message_type::heartbeat, 0, 0, 0,
             this_node_, remote_side, invalid_actor_id, invalid_actor_id,
             sequence_number};
  write(ctx, buf, hdr);
}

} // namespace basp
} // namespace io
} // namespace caf
