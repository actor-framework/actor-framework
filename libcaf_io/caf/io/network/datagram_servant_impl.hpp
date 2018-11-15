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

#include "caf/io/fwd.hpp"
#include "caf/io/datagram_servant.hpp"

#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/datagram_handler_impl.hpp"

#include "caf/policy/udp.hpp"

namespace caf {
namespace io {
namespace network {

/// Default datagram servant implementation.
class datagram_servant_impl : public datagram_servant {
  using id_type = int64_t;

public:
  datagram_servant_impl(default_multiplexer& mx, native_socket sockfd,
                        int64_t id);

  bool new_endpoint(network::receive_buffer& buf) override;

  void ack_writes(bool enable) override;

  std::vector<char>& wr_buf(datagram_handle hdl) override;

  void enqueue_datagram(datagram_handle hdl, std::vector<char> buf) override;

  network::receive_buffer& rd_buf() override;

  void graceful_shutdown() override;

  void flush() override;

  std::string addr() const override;

  uint16_t port(datagram_handle hdl) const override;

  uint16_t local_port() const override;

  std::vector<datagram_handle> hdls() const override;

  void add_endpoint(const ip_endpoint& ep, datagram_handle hdl) override;

  void remove_endpoint(datagram_handle hdl) override;

  void launch() override;

  void add_to_loop() override;

  void remove_from_loop() override;

  void detach_handles() override;

private:
  bool launched_;
  datagram_handler_impl<policy::udp> handler_;
};

} // namespace network
} // namespace io
} // namespace caf
