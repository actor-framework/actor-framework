// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>

#include "caf/detail/io_export.hpp"
#include "caf/io/datagram_servant.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/network/datagram_handler_impl.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/policy/udp.hpp"

namespace caf::io::network {

/// Default datagram servant implementation.
class CAF_IO_EXPORT datagram_servant_impl : public datagram_servant {
public:
  using id_type = int64_t;

  datagram_servant_impl(default_multiplexer& mx, native_socket sockfd,
                        id_type id);

  bool new_endpoint(network::receive_buffer& buf) override;

  void ack_writes(bool enable) override;

  byte_buffer& wr_buf(datagram_handle hdl) override;

  void enqueue_datagram(datagram_handle hdl, byte_buffer buf) override;

  network::receive_buffer& rd_buf() override;

  void graceful_shutdown() override;

  void flush() override;

  std::string addr(datagram_handle hdl) const override;

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

} // namespace caf::io::network
