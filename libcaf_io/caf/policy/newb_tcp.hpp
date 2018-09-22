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

#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/policy/accept.hpp"
#include "caf/policy/transport.hpp"

namespace caf {
namespace policy {

struct tcp_transport : public transport {
  tcp_transport();

  io::network::rw_state read_some(io::newb_base* parent) override;

  bool should_deliver() override;

  void prepare_next_read(io::newb_base*) override;

  void configure_read(io::receive_policy::config config) override;

  io::network::rw_state write_some(io::newb_base* parent) override;

  void prepare_next_write(io::newb_base* parent) override;

  void flush(io::newb_base* parent) override;

  expected<io::network::native_socket>
  connect(const std::string& host, uint16_t port,
          optional<io::network::protocol::network> preferred = none) override;

  // State for reading.
  size_t read_threshold;
  size_t collected;
  size_t maximum;
  io::receive_policy_flag rd_flag;

  // State for writing.
  bool writing;
  size_t written;
};

io::network::native_socket get_newb_socket(io::newb_base*);

template <class Message>
struct accept_tcp : public accept<Message> {
  expected<io::network::native_socket>
  create_socket(uint16_t port,const char* host,bool reuse = false) override {
    return io::network::new_tcp_acceptor_impl(port, host, reuse);
  }

  std::pair<io::network::native_socket, transport_ptr>
  accept_event(io::newb_base* parent) override {
    auto esock = io::network::accept_tcp_connection(get_newb_socket(parent));
    if (!esock) {
      return {io::network::invalid_native_socket, nullptr};
    }
    transport_ptr ptr{new tcp_transport};
    return {*esock, std::move(ptr)};
  }

  void init(io::newb_base*, io::newb<Message>& spawned) override {
    spawned.start();
  }
};

template <class T>
using tcp_protocol = generic_protocol<T>;

} // namespace policy
} // namespace caf
