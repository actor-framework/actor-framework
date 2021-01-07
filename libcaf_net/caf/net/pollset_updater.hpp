// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <array>
#include <cstdint>
#include <mutex>

#include "caf/byte.hpp"
#include "caf/net/pipe_socket.hpp"
#include "caf/net/socket_manager.hpp"

namespace caf::net {

class pollset_updater : public socket_manager {
public:
  // -- member types -----------------------------------------------------------

  using super = socket_manager;

  using msg_buf = std::array<byte, sizeof(intptr_t) + 1>;

  // -- constants --------------------------------------------------------------

  static constexpr uint8_t register_reading_code = 0x00;

  static constexpr uint8_t register_writing_code = 0x01;

  static constexpr uint8_t init_manager_code = 0x02;

  static constexpr uint8_t shutdown_code = 0x04;

  // -- constructors, destructors, and assignment operators --------------------

  pollset_updater(pipe_socket read_handle, multiplexer* parent);

  ~pollset_updater() override;

  // -- properties -------------------------------------------------------------

  /// Returns the managed socket.
  pipe_socket handle() const noexcept {
    return socket_cast<pipe_socket>(handle_);
  }

  // -- interface functions ----------------------------------------------------

  error init(const settings& config) override;

  bool handle_read_event() override;

  bool handle_write_event() override;

  void handle_error(sec code) override;

private:
  msg_buf buf_;
  size_t buf_size_ = 0;
};

} // namespace caf::net
