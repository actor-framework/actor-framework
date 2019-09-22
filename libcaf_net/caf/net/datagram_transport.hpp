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

  using factory_type = Factory;

  using transport_type = datagram_transport;

  using application_type = typename Factory::application_type;

  using dispatcher_type = transport_worker_dispatcher<factory_type,
                                                      ip_endpoint>;

  // -- constructors, destructors, and assignment operators --------------------

  datagram_transport(udp_datagram_socket handle, factory_type factory)
    : dispatcher_(std::move(factory)),
      handle_(handle),
      max_consecutive_reads_(0),
      read_threshold_(1024),
      collected_(0),
      max_(1024),
      rd_flag_(receive_policy_flag::exactly) {
    // nop
  }

  // -- public member functions ------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    if (auto err = dispatcher_.init(parent))
      return err;
    parent.mask_add(operation::read);
    return none;
  }

  template <class Parent>
  bool handle_read_event(Parent& parent) {
    CAF_LOG_TRACE(CAF_ARG(handle_.id));
    auto ret = read(handle_, make_span(read_buf_));
    if (auto res = get_if<std::pair<size_t, ip_endpoint>>(&ret)) {
      auto num_bytes = res->first;
      auto ep = res->second;
      collected_ += (num_bytes > 0) ? static_cast<size_t>(num_bytes) : 0;
      dispatcher_.handle_data(parent, make_span(read_buf_), std::move(ep));
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
  void resolve(Parent& parent, const std::string& path, actor listener) {
    dispatcher_.resolve(parent, path, listener);
  }

  template <class Parent>
  void timeout(Parent& parent, atom_value value, uint64_t id) {
    auto decorator = make_write_packet_decorator(*this, parent);
    dispatcher_.timeout(decorator, value, id);
  }

  template <class Parent>
  uint64_t set_timeout(uint64_t timeout_id, ip_endpoint ep) {
    dispatcher_.set_timeout(timeout_id, ep);
  }

  void handle_error(sec code) {
    dispatcher_.handle_error(code);
  }

  udp_datagram_socket handle() const noexcept {
    return handle_;
  }

  void prepare_next_read() {
    read_buf_.clear();
    collected_ = 0;
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

  void write_packet(span<const byte> header, span<const byte> payload,
                    typename dispatcher_type::id_type id) {
    std::vector<byte> buf;
    buf.reserve(header.size() + payload.size());
    buf.insert(buf.end(), header.begin(), header.end());
    buf.insert(buf.end(), payload.begin(), payload.end());
    packet_queue_.emplace_back(id, std::move(buf));
  }

  struct packet {
    ip_endpoint destination;
    std::vector<byte> bytes;

    packet(ip_endpoint destination, std::vector<byte> bytes)
      : destination(destination), bytes(std::move(bytes)) {
      // nop
    }
  };

private:
  bool write_some() {
    if (packet_queue_.empty())
      return false;
    auto& next_packet = packet_queue_.front();
    auto send_res = write(handle_, make_span(next_packet.bytes),
                          next_packet.destination);
    if (auto num_bytes = get_if<size_t>(&send_res)) {
      CAF_LOG_DEBUG(CAF_ARG(handle_.id) << CAF_ARG(*num_bytes));
      packet_queue_.pop_front();
      return true;
    }
    auto err = get<sec>(send_res);
    CAF_LOG_DEBUG("send failed" << CAF_ARG(err));
    dispatcher_.handle_error(err);
    return false;
  }

  dispatcher_type dispatcher_;
  udp_datagram_socket handle_;

  std::vector<byte> read_buf_;
  std::deque<packet> packet_queue_;

  size_t max_consecutive_reads_;
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  receive_policy_flag rd_flag_;
};

} // namespace net
} // namespace caf