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

#include "caf/net/pipe_socket.hpp"
#include "caf/net/socket_manager.hpp"

namespace caf {
namespace net {

class pollset_updater : public socket_manager {
public:
  // -- member types -----------------------------------------------------------

  using super = socket_manager;

  // -- constructors, destructors, and assignment operators --------------------

  pollset_updater(pipe_socket read_handle, multiplexer_ptr parent);

  ~pollset_updater() override;

  // -- properties -------------------------------------------------------------

  /// Returns the managed socket.
  pipe_socket handle() const noexcept {
    return socket_cast<pipe_socket>(handle_);
  }

  // -- interface functions ----------------------------------------------------

  bool handle_read_event() override;

  bool handle_write_event() override;

  void handle_error(sec code) override;
};

} // namespace net
} // namespace caf
