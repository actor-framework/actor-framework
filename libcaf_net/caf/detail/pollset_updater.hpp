// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/pipe_socket.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/detail/net_export.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace caf::detail {

class CAF_NET_EXPORT pollset_updater : public net::socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using super = net::socket_manager;

  using msg_buf = std::array<std::byte, sizeof(intptr_t) + 1>;

  enum class code : uint8_t {
    start_manager,
    shutdown_reading,
    shutdown_writing,
    run_action,
    shutdown,
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit pollset_updater(net::pipe_socket fd);

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<pollset_updater> make(net::pipe_socket fd);

  // -- implementation of socket_event_layer -----------------------------------

  error start(net::socket_manager* owner) override;

  net::socket handle() const override;

  void handle_read_event() override;

  void handle_write_event() override;

  void abort(const error& reason) override;

private:
  net::pipe_socket fd_;
  net::socket_manager* owner_ = nullptr;
  net::multiplexer* mpx_ = nullptr;
  msg_buf buf_;
  size_t buf_size_ = 0;
};

} // namespace caf::detail
