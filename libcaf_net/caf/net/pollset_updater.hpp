// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/net/pipe_socket.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace caf::net {

class CAF_NET_EXPORT pollset_updater : public socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using super = socket_manager;

  using msg_buf = std::array<std::byte, sizeof(intptr_t) + 1>;

  enum class code : uint8_t {
    register_reading,
    continue_reading,
    register_writing,
    continue_writing,
    init_manager,
    discard_manager,
    shutdown_reading,
    shutdown_writing,
    run_action,
    shutdown,
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit pollset_updater(pipe_socket fd);

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<pollset_updater> make(pipe_socket fd);

  // -- implementation of socket_event_layer -----------------------------------

  error init(socket_manager* owner, const settings& cfg) override;

  read_result handle_read_event() override;

  read_result handle_buffered_data() override;

  read_result handle_continue_reading() override;

  write_result handle_write_event() override;

  void abort(const error& reason) override;

private:
  pipe_socket fd_;
  multiplexer* mpx_ = nullptr;
  msg_buf buf_;
  size_t buf_size_ = 0;
};

} // namespace caf::net
