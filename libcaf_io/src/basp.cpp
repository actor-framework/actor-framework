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

#include "caf/io/basp.hpp"

#include "caf/message.hpp"
#include "caf/to_string.hpp"
#include "caf/uniform_typeid.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {
namespace io {
namespace basp {

/******************************************************************************
 *                               free functions                               *
 ******************************************************************************/

std::string to_string(message_type x) {
  switch (x) {
    case message_type::server_handshake:
      return "server_handshake";
    case message_type::client_handshake:
      return "client_handshake";
    case message_type::dispatch_message:
      return "dispatch_message";
    case message_type::announce_proxy_instance:
      return "announce_proxy_instance";
    case message_type::kill_proxy_instance:
      return "kill_proxy_instance";
    default:
      return "???";
  }
}

std::string to_string(const header &hdr) {
  std::ostringstream oss;
  oss << "{"
      << to_string(hdr.operation) << ", "
      << hdr.payload_len << ", "
      << hdr.operation_data << ", "
      << to_string(hdr.source_node) << ", "
      << to_string(hdr.dest_node) << ", "
      << hdr.source_actor << ", "
      << hdr.dest_actor
      << "}";
  return oss.str();
}

void read_hdr(deserializer& source, header& hdr) {
  uint32_t operation;
  hdr.source_node.deserialize(source);
  hdr.dest_node.deserialize(source);
  source.read(hdr.source_actor)
        .read(hdr.dest_actor)
        .read(hdr.payload_len)
        .read(operation)
        .read(hdr.operation_data);
  hdr.operation = static_cast<basp::message_type>(operation);
}

void write_hdr(serializer& sink, const header& hdr) {
  hdr.source_node.serialize(sink);
  hdr.dest_node.serialize(sink);
  sink.write(hdr.source_actor)
      .write(hdr.dest_actor)
      .write(hdr.payload_len)
      .write(static_cast<uint32_t>(hdr.operation))
      .write(hdr.operation_data);
}

bool operator==(const header& lhs, const header& rhs) {
  return lhs.operation == rhs.operation
      && lhs.payload_len == rhs.payload_len
      && lhs.operation_data == rhs.operation_data
      && lhs.source_node == rhs.source_node
      && lhs.dest_node == rhs.dest_node
      && lhs.source_actor == rhs.source_actor
      && lhs.dest_actor == rhs.dest_actor;
}

namespace {

bool valid(const node_id& val) {
  return val != invalid_node_id;
}

template <class T>
bool zero(T val) {
  return val == 0;
}

bool server_handshake_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && ! valid(hdr.dest_node)
       && zero(hdr.dest_actor)
       && ! zero(hdr.operation_data);
}

bool client_handshake_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && zero(hdr.source_actor)
       && zero(hdr.dest_actor)
       && zero(hdr.payload_len)
       && zero(hdr.operation_data);
}

bool dispatch_message_valid(const header& hdr) {
  return  valid(hdr.dest_node)
       && ! zero(hdr.dest_actor)
       && ! zero(hdr.payload_len);
}

bool announce_proxy_instance_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && zero(hdr.source_actor)
       && ! zero(hdr.dest_actor)
       && zero(hdr.payload_len)
       && zero(hdr.operation_data);
}

bool kill_proxy_instance_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && ! zero(hdr.source_actor)
       && zero(hdr.dest_actor)
       && zero(hdr.payload_len)
       && ! zero(hdr.operation_data);
}

} // namespace <anonymous>

bool valid(const header& hdr) {
  switch (hdr.operation) {
    default:
      return false; // invalid operation field
    case message_type::server_handshake:
      return server_handshake_valid(hdr);
    case message_type::client_handshake:
      return client_handshake_valid(hdr);
    case message_type::dispatch_message:
      return dispatch_message_valid(hdr);
    case message_type::announce_proxy_instance:
      return announce_proxy_instance_valid(hdr);
    case message_type::kill_proxy_instance:
      return kill_proxy_instance_valid(hdr);
  }
}

/******************************************************************************
 *                               routing_table                                *
 ******************************************************************************/

routing_table::routing_table(abstract_broker* parent) : parent_(parent) {
  // nop
}

routing_table::~routing_table() {
  // nop
}

maybe<routing_table::route> routing_table::lookup(const node_id& target) {
  auto hdl = lookup_direct(target);
  if (hdl != invalid_connection_handle)
    return route{parent_->wr_buf(hdl), target, hdl};
  // pick first available indirect route
  auto i = indirect_.find(target);
  if (i != indirect_.end()) {
    auto& hops = i->second;
    while (! hops.empty()) {
      auto& hop = *hops.begin();
      hdl = lookup_direct(hop);
      if (hdl != invalid_connection_handle)
        return route{parent_->wr_buf(hdl), hop, hdl};
      else
        hops.erase(hops.begin());
    }
  }
  return none;
}

void routing_table::flush(const route& r) {
  parent_->flush(r.hdl);
}

node_id
routing_table::lookup_direct(const connection_handle& hdl) const {
  return get_opt(direct_by_hdl_, hdl, invalid_node_id);
}

connection_handle
routing_table::lookup_direct(const node_id& nid) const {
  return get_opt(direct_by_nid_, nid, invalid_connection_handle);
}

node_id routing_table::lookup_indirect(const node_id& nid) const {
  auto i = indirect_.find(nid);
  if (i == indirect_.end())
    return invalid_node_id;
  if (i->second.empty())
    return invalid_node_id;
  return *i->second.begin();
}

void routing_table::blacklist(const node_id& hop, const node_id& dest) {
  blacklist_[dest].emplace(hop);
  auto i = indirect_.find(dest);
  if (i == indirect_.end())
    return;
  i->second.erase(hop);
  if (i->second.empty())
    indirect_.erase(i);
}

void routing_table::erase_direct(const connection_handle& hdl,
                                 erase_callback& cb) {
  auto i = direct_by_hdl_.find(hdl);
  if (i == direct_by_hdl_.end())
    return;
  cb(i->second);
  parent_->parent().notify<hook::connection_lost>(i->second);
  direct_by_nid_.erase(i->second);
  direct_by_hdl_.erase(i);
}

bool routing_table::erase_indirect(const node_id& dest) {
  auto i = indirect_.find(dest);
  if (i == indirect_.end())
    return false;
  if (parent_->parent().has_hook())
    for (auto& nid : i->second)
      parent_->parent().notify<hook::route_lost>(nid, dest);
  indirect_.erase(i);
  return true;
}

void routing_table::add_direct(const connection_handle& hdl,
                               const node_id& nid) {
  CAF_ASSERT(direct_by_hdl_.count(hdl) == 0);
  CAF_ASSERT(direct_by_nid_.count(nid) == 0);
  direct_by_hdl_.emplace(hdl, nid);
  direct_by_nid_.emplace(nid, hdl);
  parent_->parent().notify<hook::new_connection_established>(nid);
}

bool routing_table::add_indirect(const node_id& hop, const node_id& dest) {
  auto i = blacklist_.find(dest);
  if (i == blacklist_.end() || i->second.count(hop) == 0) {
    auto& hops = indirect_[dest];
    auto added_first = hops.empty();
    hops.emplace(hop);
    parent_->parent().notify<hook::new_route_added>(hop, dest);
    return added_first;
  }
  return false; // blacklisted
}

bool routing_table::reachable(const node_id& dest) {
  return direct_by_nid_.count(dest) > 0 || indirect_.count(dest) > 0;
}

size_t routing_table::erase(const node_id& dest, erase_callback& cb) {
  cb(dest);
  size_t res = 0;
  auto i = indirect_.find(dest);
  if (i != indirect_.end()) {
    res = i->second.size();
    for (auto& nid : i->second) {
      cb(nid);
      parent_->parent().notify<hook::route_lost>(nid, dest);
    }
    indirect_.erase(i);
  }
  auto hdl = lookup_direct(dest);
  if (hdl != invalid_connection_handle) {
    direct_by_hdl_.erase(hdl);
    direct_by_nid_.erase(dest);
    parent_->parent().notify<hook::connection_lost>(dest);
    ++res;
  }
  return res;
}

/******************************************************************************
 *                                   callee                                   *
 ******************************************************************************/

instance::callee::callee(actor_namespace::backend& mgm, middleman& mm)
    : namespace_(mgm),
      middleman_(mm) {
  // nop
}

instance::callee::~callee() {
  // nop
}

/******************************************************************************
 *                                  instance                                  *
 ******************************************************************************/

instance::instance(abstract_broker* parent, callee& lstnr)
    : tbl_(parent),
      this_node_(caf::detail::singletons::get_node_id()),
      callee_(lstnr) {
  CAF_ASSERT(this_node_ != invalid_node_id);
}

connection_state instance::handle(const new_data_msg& dm, header& hdr,
                                  bool is_payload) {
  CAF_LOG_TRACE("");
  // function object providing cleanup code on errors
  auto err = [&]() -> connection_state {
    auto cb = make_callback([&](const node_id& nid){
      callee_.purge_state(nid);
    });
    tbl_.erase_direct(dm.handle, cb);
    return close_connection;
  };
  const std::vector<char>* payload = nullptr;
  if (is_payload) {
    payload = &dm.buf;
    if (payload->size() != hdr.payload_len) {
      CAF_LOG_WARNING("received invalid payload");
      return err();
    }
  } else {
    binary_deserializer bd{dm.buf.data(), dm.buf.size(), &get_namespace()};
    read_hdr(bd, hdr);
    if (hdr.payload_len > 0)
      return await_payload;
  }
  CAF_LOG_DEBUG("hdr = " << to_string(hdr));
  if (! valid(hdr)) {
    CAF_LOG_WARNING("received invalid header of type "
                    << to_string(hdr.operation));
    return err();
  }
  // needs forwarding?
  if (! is_handshake(hdr) && hdr.dest_node != this_node_) {
    auto path = lookup(hdr.dest_node);
    if (path) {
      binary_serializer bs{std::back_inserter(path->wr_buf), &get_namespace()};
      write_hdr(bs, hdr);
      if (payload)
        bs.write_raw(payload->size(), payload->data());
      tbl_.flush(*path);
      notify<hook::message_forwarded>(hdr, payload);
    } else {
      CAF_LOG_INFO("cannot forward message, no route to destination");
      if (hdr.source_node != this_node_) {
        auto reverse_path = lookup(hdr.source_node);
        if (! reverse_path) {
          CAF_LOG_WARNING("cannot send error message: no route to source");
        } else {
          write_dispatch_error(reverse_path->wr_buf,
                               this_node_,
                               hdr.source_node,
                               error::no_route_to_destination,
                               hdr,
                               payload);
        }
      } else {
        CAF_LOG_WARNING("lost packet with probably spoofed source");
      }
      notify<hook::message_forwarding_failed>(hdr, payload);
    }
    return await_header;
  }
  // function object for checking payload validity
  auto payload_valid = [&]() -> bool {
    return payload != nullptr && payload->size() == hdr.payload_len;
  };
  // handle message to ourselves
  switch (hdr.operation) {
    case message_type::server_handshake: {
      actor_id aid = invalid_actor_id;
      std::set<std::string> sigs;
      if (payload_valid()) {
        binary_deserializer bd{payload->data(), payload->size(),
                               &get_namespace()};
        bd >> aid >> sigs;
      }
      // close self connection after handshake is done
      if (hdr.source_node == this_node_) {
        CAF_LOG_INFO("close connection to self immediately");
        callee_.finalize_handshake(hdr.source_node, aid, sigs);
        return err();
      }
      // close this connection if we already have a direct connection
      if (tbl_.lookup_direct(hdr.source_node) != invalid_connection_handle) {
        CAF_LOG_INFO("close connection since we already have a "
                     "direct connection to " << to_string(hdr.source_node));
        callee_.finalize_handshake(hdr.source_node, aid, sigs);
        return err();
      }
      // add direct route to this node and remove any indirect entry
      CAF_LOG_INFO("new direct connection: " << to_string(hdr.source_node));
      tbl_.add_direct(dm.handle, hdr.source_node);
      auto was_indirect = tbl_.erase_indirect(hdr.source_node);
      // write handshake as client in response
      auto path = tbl_.lookup(hdr.source_node);
      if (!path) {
        CAF_LOG_ERROR("no route to host after server handshake");
        return err();
      }
      write_client_handshake(path->wr_buf, hdr.source_node);
      callee_.learned_new_node_directly(hdr.source_node, was_indirect);
      callee_.finalize_handshake(hdr.source_node, aid, sigs);
      flush(*path);
      break;
    }
    case message_type::client_handshake: {
      if (tbl_.lookup_direct(hdr.source_node) != invalid_connection_handle) {
        CAF_LOG_INFO("received second client handshake for "
                     << to_string(hdr.source_node) << " (ignored)");
        break;
      }
      // add direct route to this node and remove any indirect entry
      CAF_LOG_INFO("new direct connection: " << to_string(hdr.source_node));
      tbl_.add_direct(dm.handle, hdr.source_node);
      auto was_indirect = tbl_.erase_indirect(hdr.source_node);
      callee_.learned_new_node_directly(hdr.source_node, was_indirect);
      break;
    }
    case message_type::dispatch_message: {
      if (! payload_valid())
        return err();
      // in case the sender of this message was received via a third node,
      // we assume that that node to offers a route to the original source
      auto last_hop = tbl_.lookup_direct(dm.handle);
      if (hdr.source_node != invalid_node_id
          && hdr.source_node != this_node_
          && last_hop != hdr.source_node
          && tbl_.lookup_direct(hdr.source_node) == invalid_connection_handle
          && tbl_.add_indirect(last_hop, hdr.source_node))
        callee_.learned_new_node_indirectly(hdr.source_node);
      binary_deserializer bd{payload->data(), payload->size(),
                             &get_namespace()};
      message msg;
      msg.deserialize(bd);
      callee_.deliver(hdr.source_node, hdr.source_actor,
                      hdr.dest_node, hdr.dest_actor, msg,
                      message_id::from_integer_value(hdr.operation_data));
      break;
    }
    case message_type::announce_proxy_instance:
      callee_.proxy_announced(hdr.source_node, hdr.dest_actor);
      break;
    case message_type::kill_proxy_instance:
      callee_.kill_proxy(hdr.source_node, hdr.source_actor,
                         static_cast<uint32_t>(hdr.operation_data));
      break;
    default:
      CAF_LOG_ERROR("invalid operation");
      return err();
  }
  return await_header;
}

void instance::handle(const connection_closed_msg& msg) {
  auto cb = make_callback([&](const node_id& nid){
    callee_.purge_state(nid);
  });
  tbl_.erase_direct(msg.handle, cb);
}

void instance::handle_node_shutdown(const node_id& affected_node) {
  CAF_LOG_TRACE(CAF_TSARG(affected_node));
  if (affected_node == invalid_node_id)
    return;
  CAF_LOG_INFO("lost direct connection to " << to_string(affected_node));
  auto cb = make_callback([&](const node_id& nid){
    callee_.purge_state(nid);
  });
  tbl_.erase(affected_node, cb);
}

maybe<routing_table::route> instance::lookup(const node_id& target) {
  return tbl_.lookup(target);
}

void instance::flush(const routing_table::route& path) {
  tbl_.flush(path);
}

void instance::write(const routing_table::route& r, header& hdr,
                     payload_writer* writer) {
  CAF_ASSERT(hdr.payload_len == 0 || writer != nullptr);
  write(r.wr_buf, hdr, writer);
  tbl_.flush(r);
}

void instance::add_published_actor(uint16_t port,
                                   actor_addr published_actor,
                                   std::set<std::string> published_interface) {
  using std::swap;
  auto& entry = published_actors_[port];
  swap(entry.first, published_actor);
  swap(entry.second, published_interface);
  notify<hook::actor_published>(entry.first, entry.second, port);
}

size_t instance::remove_published_actor(uint16_t port,
                                        removed_published_actor* cb) {
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
  size_t result = 0;
  if (port != 0) {
    auto i = published_actors_.find(port);
    if (i != published_actors_.end() && i->second.first == whom) {
      if (cb)
        (*cb)(whom, port);
      published_actors_.erase(i);
      result = 1;
    }
  } else {
    auto i = published_actors_.begin();
    while (i != published_actors_.end()) {
      if (i->second.first == whom) {
        if (cb)
          (*cb)(whom, i->first);
        i = published_actors_.erase(i);
        ++result;
      } else {
        ++i;
      }
    }
  }
  return result;
}

bool instance::dispatch(const actor_addr& sender, const actor_addr& receiver,
                        message_id mid, const message& msg) {
  CAF_LOG_TRACE("");
  CAF_ASSERT(receiver.is_remote());
  auto path = lookup(receiver->node());
  if (! path) {
    notify<hook::message_sending_failed>(sender, receiver, mid, msg);
    return false;
  }
  auto writer = make_callback([&](serializer& sink) {
    msg.serialize(sink);
  });
  header hdr{message_type::dispatch_message, 0, mid.integer_value(),
             sender ? sender->node() : this_node(), receiver->node(),
             sender ? sender->id() : invalid_actor_id, receiver->id()};
  write(path->wr_buf, hdr, &writer);
  flush(*path);
  notify<hook::message_sent>(sender, path->next_hop, receiver, mid, msg);
  return true;
}

void instance::write(buffer_type& buf,
                     message_type operation,
                     uint32_t* payload_len,
                     uint64_t operation_data,
                     const node_id& source_node,
                     const node_id& dest_node,
                     actor_id source_actor,
                     actor_id dest_actor,
                     payload_writer* pw) {
  if (! pw) {
    binary_serializer bs{std::back_inserter(buf), &get_namespace()};
    source_node.serialize(bs);
    dest_node.serialize(bs);
    bs.write(source_actor)
      .write(dest_actor)
      .write(uint32_t{0})
      .write(static_cast<uint32_t>(operation))
      .write(operation_data);
  } else {
    // reserve space in the buffer to write the payload later on
    auto wr_pos = static_cast<ptrdiff_t>(buf.size());
    char placeholder[basp::header_size];
    buf.insert(buf.end(), std::begin(placeholder), std::end(placeholder));
    auto pl_pos = buf.size();
    { // lifetime scope of first serializer (write payload)
      binary_serializer bs1{std::back_inserter(buf), &get_namespace()};
      (*pw)(bs1);
    }
    // write broker message to the reserved space
    binary_serializer bs2{buf.begin() + wr_pos, &get_namespace()};
    auto plen = static_cast<uint32_t>(buf.size() - pl_pos);
    source_node.serialize(bs2);
    dest_node.serialize(bs2);
    bs2.write(source_actor)
       .write(dest_actor)
       .write(plen)
       .write(static_cast<uint32_t>(operation))
       .write(operation_data);
    if (payload_len)
      *payload_len = plen;
  }
}

void instance::write(buffer_type& buf, header& hdr, payload_writer* pw) {
  write(buf, hdr.operation, &hdr.payload_len, hdr.operation_data,
        hdr.source_node, hdr.dest_node, hdr.source_actor, hdr.dest_actor, pw);
}

void instance::write_server_handshake(buffer_type& out_buf,
                                      maybe<uint16_t> port) {
  using namespace detail;
  published_actor* pa = nullptr;
  if (port) {
    auto i = published_actors_.find(*port);
    if (i != published_actors_.end())
      pa = &i->second;
  }
  auto writer = make_callback([&](serializer& sink) {
    if (pa)
      sink << pa->first.id() << pa->second;
  });
  header hdr{message_type::server_handshake, 0, version,
             this_node_, invalid_node_id,
             pa ? pa->first.id() : invalid_actor_id, invalid_actor_id};
  write(out_buf, hdr, &writer);
}

void instance::write_client_handshake(buffer_type& buf,
                                      const node_id& remote_side) {
  write(buf,
        message_type::client_handshake, nullptr, 0,
        this_node_, remote_side,
        invalid_actor_id, invalid_actor_id);
}

void instance::write_dispatch_error(buffer_type& buf,
                                    const node_id& source_node,
                                    const node_id& dest_node,
                                    error error_code,
                                    const header& original_hdr,
                                    const buffer_type* payload) {
  auto writer = make_callback([&](serializer& sink) {
    write_hdr(sink, original_hdr);
    if (payload)
      sink.write_raw(payload->size(), payload->data());
  });
  header hdr{message_type::kill_proxy_instance, 0,
             static_cast<uint64_t>(error_code),
             source_node, dest_node,
             invalid_actor_id, invalid_actor_id};
  write(buf, hdr, &writer);
}

void instance::write_kill_proxy_instance(buffer_type& buf,
                                         const node_id& dest_node,
                                         actor_id aid,
                                         uint32_t rsn) {
  header hdr{message_type::kill_proxy_instance, 0, rsn,
             this_node_, dest_node,
             aid, invalid_actor_id};
  write(buf, hdr);
}

} // namespace basp
} // namespace io
} // namespace caf
