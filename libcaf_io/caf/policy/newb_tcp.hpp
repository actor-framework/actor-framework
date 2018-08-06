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

#include "caf/io/network/newb.hpp"
#include "caf/io/network/native_socket.hpp"

namespace caf {
namespace policy {

struct tcp_transport : public io::network::transport_policy {
  tcp_transport();

  error read_some(io::network::event_handler* parent) override;

  bool should_deliver() override;

  void prepare_next_read(io::network::event_handler*) override;

  void configure_read(io::receive_policy::config config) override;

  error write_some(io::network::event_handler* parent) override;

  void prepare_next_write(io::network::event_handler* parent) override;

  inline io::network::byte_buffer& wr_buf() {
    return offline_buffer;
  }

  void flush(io::network::event_handler* parent) override;

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

struct accept_tcp : public io::network::accept_policy {
  expected<io::network::native_socket>
  create_socket(uint16_t port,const char* host,bool reuse = false) override;

  std::pair<io::network::native_socket, io::network::transport_policy_ptr>
  accept(io::network::event_handler* parent) override;

  void init(io::network::newb_base& n) override;
};

template <class T>
struct tcp_protocol
    : public io::network::protocol_policy<typename T::message_type> {
  T impl;

  tcp_protocol(io::network::newb<typename T::message_type>* parent)
      : impl(parent) {
    // nop
  }

  error read(char* bytes, size_t count) override {
    return impl.read(bytes, count);
  }

  error timeout(atom_value atm, uint32_t id) override {
    return impl.timeout(atm, id);
  }

  void write_header(io::network::byte_buffer& buf,
                    io::network::header_writer* hw) override {
    impl.write_header(buf, hw);
  }

  void prepare_for_sending(io::network::byte_buffer& buf, size_t hstart,
                           size_t offset, size_t plen) override {
    impl.prepare_for_sending(buf, hstart, offset, plen);
  }
};

} // namespace policy
} // namespace caf
