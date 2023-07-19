// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/doorman.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/network/acceptor_impl.hpp"
#include "caf/io/network/native_socket.hpp"

#include "caf/detail/io_export.hpp"
#include "caf/policy/tcp.hpp"

namespace caf::io::network {

/// Default doorman implementation.
class CAF_IO_EXPORT doorman_impl : public doorman {
public:
  doorman_impl(default_multiplexer& mx, native_socket sockfd);

  bool new_connection() override;

  void graceful_shutdown() override;

  void launch() override;

  std::string addr() const override;

  uint16_t port() const override;

  void add_to_loop() override;

  void remove_from_loop() override;

protected:
  acceptor_impl<policy::tcp> acceptor_;
};

} // namespace caf::io::network
