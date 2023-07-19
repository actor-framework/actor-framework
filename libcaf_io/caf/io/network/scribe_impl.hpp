// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/fwd.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/stream_impl.hpp"
#include "caf/io/scribe.hpp"

#include "caf/detail/io_export.hpp"
#include "caf/policy/tcp.hpp"

namespace caf::io::network {

/// Default scribe implementation.
class CAF_IO_EXPORT scribe_impl : public scribe {
public:
  scribe_impl(default_multiplexer& mx, native_socket sockfd);

  void configure_read(receive_policy::config config) override;

  void ack_writes(bool enable) override;

  byte_buffer& wr_buf() override;

  byte_buffer& rd_buf() override;

  void graceful_shutdown() override;

  void flush() override;

  std::string addr() const override;

  uint16_t port() const override;

  void launch();

  void add_to_loop() override;

  void remove_from_loop() override;

protected:
  bool launched_;
  stream_impl<policy::tcp> stream_;
};

} // namespace caf::io::network
