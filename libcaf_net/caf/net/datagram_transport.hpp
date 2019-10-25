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

#pragma once

#include <deque>
#include <unordered_map>

#include "caf/byte.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/logger.hpp"
#include "caf/net/defaults.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/transport_worker_dispatcher.hpp"
#include "caf/net/udp_datagram_socket.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

/// Implements a udp_transport policy that manages a datagram socket.
template <class Factory>
class datagram_transport {
public:
  // -- member types -----------------------------------------------------------

  using id_type = ip_endpoint;

  using buffer_type = std::vector<byte>;

  using buffer_cache_type = std::vector<buffer_type>;

  using factory_type = Factory;

  using transport_type = datagram_transport<Factory>;

  using dispatcher_type = transport_worker_dispatcher<transport_type, id_type>;

  using application_type = typename Factory::application_type;

  // -- constructors, destructors, and assignment operators --------------------

  datagram_transport(udp_datagram_socket handle, factory_type factory)
    : dispatcher_(*this, std::move(factory)),
      handle_(handle),
      max_consecutive_reads_(0),
      read_threshold_(1024),
      max_(1024),
      rd_flag_(receive_policy_flag::exactly),
      manager_(nullptr) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  udp_datagram_socket handle() const noexcept {
    return handle_;
  }

  actor_system& system() {
    return manager().system();
  }

  application_type& application() {
    // TODO: This wont work. We need information on which application is wanted
    return application_type{};
    // dispatcher_.application();
  }

  transport_type& transport() {
    return *this;
  }

  endpoint_manager& manager() {
    return *manager_;
  }

  // -- public member functions ------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    manager_ = &parent;
    auto& cfg = system().config();
    auto max_header_bufs = get_or(cfg, "middleman.max-header-buffers",
                                  defaults::middleman::max_header_buffers);
    header_bufs_.reserve(max_header_bufs);
    auto max_payload_bufs = get_or(cfg, "middleman.max-payload-buffers",
                                   defaults::middleman::max_payload_buffers);
    payload_bufs_.reserve(max_payload_bufs);
    if (auto err = dispatcher_.init(*this))
      return err;
    return none;
  }

  template <class Parent>
  bool handle_read_event(Parent&) {
    CAF_LOG_TRACE(CAF_ARG(handle_.id));
    auto ret = read(handle_, make_span(read_buf_));
    if (auto res = get_if<std::pair<size_t, ip_endpoint>>(&ret)) {
      auto num_bytes = res->first;
      auto ep = res->second;
      read_buf_.resize(num_bytes);
      dispatcher_.handle_data(*this, make_span(read_buf_), std::move(ep));
      prepare_next_read();
    } else {
      auto err = get<sec>(ret);
      CAF_LOG_DEBUG("send failed" << CAF_ARG(err));
      dispatcher_.handle_error(err);
      return false;
    }
    return true;
  }

  template <class Parent>
  bool handle_write_event(Parent& parent) {
    CAF_LOG_TRACE(CAF_ARG(handle_.id)
                  << CAF_ARG2("queue-size", packet_queue_.size()));
    // Try to write leftover data.
    write_some();
    // Get new data from parent.
    for (auto msg = parent.next_message(); msg != nullptr;
         msg = parent.next_message()) {
      dispatcher_.write_message(*this, std::move(msg));
    }
    // Write prepared data.
    return write_some();
  }

  template <class Parent>
  void resolve(Parent&, const uri& locator, const actor& listener) {
    dispatcher_.resolve(*this, locator, listener);
  }

  template <class Parent>
  void new_proxy(Parent&, const node_id& peer, actor_id id) {
    dispatcher_.new_proxy(*this, peer, id);
  }

  template <class Parent>
  void local_actor_down(Parent&, const node_id& peer, actor_id id,
                        error reason) {
    dispatcher_.local_actor_down(*this, peer, id, std::move(reason));
  }

  template <class Parent>
  void timeout(Parent&, atom_value value, uint64_t id) {
    dispatcher_.timeout(*this, value, id);
  }

  void set_timeout(uint64_t timeout_id, id_type id) {
    dispatcher_.set_timeout(timeout_id, id);
  }

  void handle_error(sec code) {
    dispatcher_.handle_error(code);
  }

  error add_new_worker(node_id node, id_type id) {
    return dispatcher_.add_new_worker(*this, node, id);
  }

  void prepare_next_read() {
    read_buf_.clear();
    // This cast does nothing, but prevents a weird compiler error on GCC
    // <= 4.9.
    // TODO: remove cast when dropping support for GCC 4.9.
    switch (static_cast<receive_policy_flag>(rd_flag_)) {
      case receive_policy_flag::exactly:
        if (read_buf_.size() != max_)
          read_buf_.resize(max_);
        read_threshold_ = max_;
        break;
      case receive_policy_flag::at_most:
        if (read_buf_.size() != max_)
          read_buf_.resize(max_);
        read_threshold_ = 1;
        break;
      case receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto max_size = max_ + std::max<size_t>(100, max_ / 10);
        if (read_buf_.size() != max_size)
          read_buf_.resize(max_size);
        read_threshold_ = max_;
        break;
      }
    }
  }

  void configure_read(receive_policy::config cfg) {
    rd_flag_ = cfg.first;
    max_ = cfg.second;
    prepare_next_read();
  }

  void write_packet(id_type id, span<buffer_type*> buffers) {
    CAF_ASSERT(!buffers.empty());
    if (packet_queue_.empty())
      manager().register_writing();
    // By convention, the first buffer is a header buffer. Every other buffer is
    // a payload buffer.
    packet_queue_.emplace_back(id, buffers);
  }

  // -- buffer management ------------------------------------------------------

  buffer_type next_header_buffer() {
    return next_buffer_impl(header_bufs_);
  }

  buffer_type next_payload_buffer() {
    return next_buffer_impl(payload_bufs_);
  }

  /// Helper struct for managing outgoing packets
  struct packet {
    id_type id;
    buffer_cache_type bytes;
    size_t size;

    packet(id_type id, span<buffer_type*> bufs) : id(id) {
      size = 0;
      for (auto buf : bufs) {
        size += buf->size();
        bytes.emplace_back(std::move(*buf));
      }
    }
  };

private:
  // -- utility functions ------------------------------------------------------

  static buffer_type next_buffer_impl(buffer_cache_type cache) {
    if (cache.empty()) {
      return {};
    }
    auto buf = std::move(cache.back());
    cache.pop_back();
    return buf;
  }

  bool write_some() {
    CAF_LOG_TRACE(CAF_ARG(handle_.id));
    // Helper function to sort empty buffers back into the right caches.
    auto recycle = [&]() {
      auto& front = packet_queue_.front();
      auto& bufs = front.bytes;
      auto it = bufs.begin();
      if (header_bufs_.size() < header_bufs_.capacity()) {
        it->clear();
        header_bufs_.emplace_back(std::move(*it++));
      }
      for (;
           it != bufs.end() && payload_bufs_.size() < payload_bufs_.capacity();
           ++it) {
        it->clear();
        payload_bufs_.emplace_back(std::move(*it));
      }
      packet_queue_.pop_front();
    };
    // Write as many bytes as possible.
    while (!packet_queue_.empty()) {
      auto& packet = packet_queue_.front();
      std::vector<std::vector<byte>*> ptrs;
      for (auto& buf : packet.bytes)
        ptrs.emplace_back(&buf);
      auto write_ret = write(handle_, make_span(ptrs), packet.id);
      if (auto num_bytes = get_if<size_t>(&write_ret)) {
        CAF_LOG_DEBUG(CAF_ARG(handle_.id) << CAF_ARG(*num_bytes));
        CAF_LOG_WARNING_IF(*num_bytes < packet.size,
                           "packet was not sent completely");
        recycle();
      } else {
        auto err = get<sec>(write_ret);
        if (err != sec::unavailable_or_would_block) {
          CAF_LOG_DEBUG("send failed" << CAF_ARG(err));
          dispatcher_.handle_error(err);
          return false;
        }
        return true;
      }
    }
    return false;
  }

  dispatcher_type dispatcher_;
  udp_datagram_socket handle_;

  buffer_cache_type header_bufs_;
  buffer_cache_type payload_bufs_;

  std::vector<byte> read_buf_;
  std::deque<packet> packet_queue_;

  size_t max_consecutive_reads_;
  size_t read_threshold_;

  size_t max_;
  receive_policy_flag rd_flag_;

  endpoint_manager* manager_;
};

} // namespace net
} // namespace caf
