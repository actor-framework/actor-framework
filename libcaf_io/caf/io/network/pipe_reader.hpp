// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/fwd.hpp"

#include "caf/detail/io_export.hpp"
#include "caf/io/network/event_handler.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/operation.hpp"

namespace caf::io::network {

/// An event handler for the internal event pipe.
class CAF_IO_EXPORT pipe_reader : public event_handler {
public:
  pipe_reader(default_multiplexer& dm);

  void removed_from_loop(operation op) override;

  void graceful_shutdown() override;

  void handle_event(operation op) override;

  void init(native_socket sock_fd);

  resumable* try_read_next();
};

} // namespace caf::io::network
