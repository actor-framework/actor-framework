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

#pragma once

#include "caf/io/network/acceptor_base.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/policy/accept.hpp"
#include "caf/policy/transport.hpp"

namespace caf {
namespace policy {

struct udp_transport : public transport {
  udp_transport();

  io::network::rw_state read_some(io::network::newb_base* parent) override;

  inline bool should_deliver() override {
    CAF_LOG_TRACE("");
    return received_bytes != 0 && sender == endpoint;
  }

  void prepare_next_read(io::network::newb_base*) override;

  inline void configure_read(io::receive_policy::config) override {
    // nop
  }

  io::network::rw_state write_some(io::network::newb_base* parent) override;

  void prepare_next_write(io::network::newb_base* parent) override;

  byte_buffer& wr_buf() override;

  void flush(io::network::newb_base* parent) override;

  expected<io::network::native_socket>
  connect(const std::string& host, uint16_t port,
          optional<io::network::protocol::network> preferred = none) override;

  void shutdown(io::network::newb_base* parent,
                io::network::native_socket sockfd) override;

  // State for reading.
  size_t maximum;
  bool first_message;

  // State for writing.
  bool writing;
  size_t written;
  size_t offline_sum;
  std::deque<size_t> send_sizes;
  std::deque<size_t> offline_sizes;

  // UDP endpoints.
  io::network::ip_endpoint endpoint;
  io::network::ip_endpoint sender;
};

template <class Message>
struct accept_udp : public accept<Message> {
  expected<io::network::native_socket>
  create_socket(uint16_t port, const char* host, bool reuse = false) override {
    auto res = io::network::new_local_udp_endpoint_impl(port, host, reuse);
    if (!res)
      return std::move(res.error());
    return (*res).first;
  }

  std::pair<io::network::native_socket, transport_ptr>
  accept_event(io::network::acceptor_base*) override {
    std::cout << "Accepting msg from UDP endpoint" << std::endl;
    auto res = io::network::new_local_udp_endpoint_impl(0, nullptr, true);
    if (!res) {
      CAF_LOG_DEBUG("failed to create local endpoint" << CAF_ARG(res.error()));
      return {io::network::invalid_native_socket, nullptr};
    }
    auto sock = std::move(res->first);
    transport_ptr ptr{new udp_transport};
    return {sock, std::move(ptr)};
  }

  void init(io::network::acceptor_base* parent,
            io::newb<Message>& spawned) override {
    spawned.trans->prepare_next_read(parent);
    spawned.trans->read_some(parent, *spawned.proto.get());
    spawned.start();
  }

  void shutdown(io::network::acceptor_base* parent,
                io::network::native_socket) override {
    parent->passivate();
  }
};

template <class T>
using udp_protocol = generic_protocol<T>;

} // namespace policy
} // namespace caf
