/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include <iterator>

#include "caf/io/basp/instance.hpp"

#include "caf/variant.hpp"
#include "caf/streambuf.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/actor_system_config.hpp"

#include "caf/io/basp/version.hpp"

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
      callee_(lstnr),
      flush_(parent),
      wr_buf_(parent),
      seq_num_(lstnr) {
  CAF_ASSERT(this_node_ != none);
}

connection_state instance::handle(execution_unit* ctx, new_data_msg& dm,
                                  header& hdr, bool is_payload) {
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
      CAF_LOG_WARNING("received invalid payload");
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
      binary_serializer bs{ctx, apply_visitor(wr_buf_, path->hdl)};
      auto e = bs(hdr);
      if (e)
        return err();
      if (payload)
        bs.apply_raw(payload->size(), payload->data());
      tbl_.flush(*path);
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
  if (!handle_msg(ctx, dm.handle, hdr, payload, true, none, none))
    return err();
  return await_header;
}

bool instance::handle(execution_unit* ctx, new_datagram_msg& dm,
                      endpoint_context& ep) {
  using itr_t = std::vector<char>::iterator;
  // function object providing cleanup code on errors
  auto err = [&]() -> bool {
    auto cb = make_callback([&](const node_id& nid) -> error {
      callee_.purge_state(nid);
      return none;
    });
    tbl_.erase(dm.handle, cb);
    return false;
  };
  // extract payload
  std::vector<char> pl_buf{std::move_iterator<itr_t>(std::begin(dm.buf) +
                                                     basp::header_size),
                           std::move_iterator<itr_t>(std::end(dm.buf))};
  // resize header
  dm.buf.resize(basp::header_size);
  // extract header
  binary_deserializer bd{ctx, dm.buf};
  auto e = bd(ep.hdr);
  if (e || !valid(ep.hdr)) {
    CAF_LOG_WARNING("received invalid header:" << CAF_ARG(ep.hdr));
    std::cerr << "[!!] invalid header!" << std::endl;
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
  // TODO: Ordering
  std::cerr << "[<<] '" << to_string(ep.hdr.operation)
            << "' with seq '" << ep.hdr.sequence_number << "'" << std::endl;
  if (ep.hdr.sequence_number != ep.seq_incoming) {
    std::cerr << "[!!] '" << to_string(ep.hdr.operation) << "' with seq '"
              << ep.hdr.sequence_number << "' (!= " << ep.seq_incoming
              << ")" << std::endl;
    auto s = ep.hdr.sequence_number;
    auto h = std::move(ep.hdr);
    auto b = std::move(pl_buf);
    ep.pending.emplace(s, std::make_pair(std::move(h), std::move(b)));
    return true;
  }
  ep.seq_incoming += 1;

  // TODO: Reliability
  if (!handle_msg(ctx, dm.handle, ep.hdr, payload, false, ep, none))
    return err();
  auto itr = ep.pending.find(ep.seq_incoming);
  while (itr != ep.pending.end()) {
    ep.hdr = std::move(itr->second.first);
    pl_buf = std::move(itr->second.second);
    payload = &pl_buf;
    if (!handle_msg(ctx, get<dgram_scribe_handle>(ep.hdl),
                    ep.hdr, payload, false, ep, none))
      err();
    ep.pending.erase(itr);
    ep.seq_incoming += 1;
    itr = ep.pending.find(ep.seq_incoming);
  }
  return true;
};


bool instance::handle(execution_unit* ctx, new_endpoint_msg& em,
                      endpoint_context& ep) {
  using itr_t = std::vector<char>::iterator;
  // function object providing cleanup code on errors
  auto err = [&]() -> bool {
    auto cb = make_callback([&](const node_id& nid) -> error {
      callee_.purge_state(nid);
      return none;
    });
    tbl_.erase(em.handle, cb);
    return false;
  };
  // extract payload
  std::vector<char> pl_buf{std::move_iterator<itr_t>(std::begin(em.buf)
                                                     + basp::header_size),
                           std::move_iterator<itr_t>(std::end(em.buf))};
  // resize header
  em.buf.resize(basp::header_size);
  // extract header
  binary_deserializer bd{ctx, em.buf};
  auto e = bd(ep.hdr);
  // client handshake for UDP should be sequence number 0
  if (e || !valid(ep.hdr)) {
    CAF_LOG_WARNING("received invalid header:" << CAF_ARG(ep.hdr));
    std::cerr << "[<<] invalid header!" << std::endl;
    return err();
  }
  if (ep.hdr.sequence_number != ep.seq_incoming) {
    CAF_LOG_WARNING("Handshake with unexected sequence number: "
                    << CAF_ARG(ep.hdr.sequence_number));
    std::cerr << "[<<] Unexpected sequence number '" << ep.hdr.sequence_number
              << "'in client handshake" << std::endl;
    ep.seq_incoming = ep.hdr.sequence_number + 1;
  } else {
    std::cerr << "[<<] '" << to_string(ep.hdr.operation) << "' with seq '"
              << ep.hdr.sequence_number << "'" << std::endl;
    ep.seq_incoming += 1;
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
  if (!handle_msg(ctx, em.handle, ep.hdr, payload, false, ep, em.port))
    return err();
  // TODO: Check for pending messages ...
  return true;
}

void instance::handle_heartbeat(execution_unit* ctx) {
  CAF_LOG_TRACE("");
  for (auto& kvp: tbl_.direct_by_hdl_) {
    CAF_LOG_TRACE(CAF_ARG(kvp.first) << CAF_ARG(kvp.second));
    auto seq = apply_visitor(seq_num_, kvp.first);
    write_heartbeat(ctx, apply_visitor(wr_buf_, kvp.first), kvp.second, seq);
    apply_visitor(flush_, kvp.first);
  }
}

void instance::handle_node_shutdown(const node_id& affected_node) {
  CAF_LOG_TRACE(CAF_ARG(affected_node));
  if (affected_node == none)
    return;
  CAF_LOG_INFO("lost direct connection:" << CAF_ARG(affected_node));
  auto cb = make_callback([&](const node_id& nid) -> error {
    callee_.purge_state(nid);
    return none;
  });
  tbl_.erase(affected_node, cb);
}

optional<routing_table::endpoint> instance::lookup(const node_id& target) {
  return tbl_.lookup(target);
}

void instance::flush(const routing_table::endpoint& path) {
  tbl_.flush(path);
}

void instance::write(execution_unit* ctx, const routing_table::endpoint& r,
                     header& hdr, payload_writer* writer) {
  CAF_LOG_TRACE(CAF_ARG(hdr));
  CAF_ASSERT(hdr.payload_len == 0 || writer != nullptr);
  write(ctx, apply_visitor(wr_buf_, r.hdl), hdr, writer);
  tbl_.flush(r);
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
  if (cb)
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
      if (cb)
        (*cb)(i->second.first, port);
      published_actors_.erase(i);
      result = 1;
    }
  } else {
    auto i = published_actors_.begin();
    while (i != published_actors_.end()) {
      if (i->second.first == whom) {
        if (cb)
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
             sender ? sender->id() : invalid_actor_id, receiver->id(),
             apply_visitor(seq_num_, path->hdl)};
  write(ctx, apply_visitor(wr_buf_, path->hdl), hdr, &writer);
  flush(*path);
  notify<hook::message_sent>(sender, path->next_hop, receiver, mid, msg);
  return true;
}

void instance::write(execution_unit* ctx, buffer_type& buf,
                     header& hdr, payload_writer* pw) {
  CAF_LOG_TRACE(CAF_ARG(hdr));
  std::cerr << "[>>] '" << to_string(hdr.operation)
            << "' with seq '" << hdr.sequence_number << "'"
            << std::endl;
  error err;
  if (pw) {
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
  if (err) {
    CAF_LOG_ERROR(CAF_ARG(err));
    std::cerr << "With error: " << to_string(err) << std::endl;
  }
}

void instance::write_server_handshake(execution_unit* ctx, buffer_type& buf,
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
    auto& ref = callee_.system().config().middleman_app_identifier;
    auto e = sink(const_cast<std::string&>(ref));
    if (e)
      return e;
    if (pa) {
      auto i = pa->first ? pa->first->id() : invalid_actor_id;
      return sink(i, pa->second);
    } else {
      auto aid = invalid_actor_id;
      std::set<std::string> tmp;
      return sink(aid, tmp);
    }
  });
  header hdr{message_type::server_handshake, 0, 0, version,
             this_node_, none,
             pa && pa->first ? pa->first->id() : invalid_actor_id,
             invalid_actor_id, sequence_number};
  write(ctx, buf, hdr, &writer);
}

void instance::write_client_handshake(execution_unit* ctx, buffer_type& buf,
                                      const node_id& remote_side,
                                      uint16_t sequence_number) {
  CAF_LOG_TRACE(CAF_ARG(remote_side));
  auto writer = make_callback([&](serializer& sink) -> error {
    auto& str = callee_.system().config().middleman_app_identifier;
    return sink(const_cast<std::string&>(str));
  });
  header hdr{message_type::client_handshake, 0, 0, 0,
             this_node_, remote_side, invalid_actor_id, invalid_actor_id,
             sequence_number};
  write(ctx, buf, hdr, &writer);
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

void instance::write_heartbeat(execution_unit* ctx, buffer_type& buf,
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
