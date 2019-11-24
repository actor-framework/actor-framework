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
