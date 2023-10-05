// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/fwd.hpp"
#include "caf/net/octet_stream/lower_layer.hpp"
#include "caf/net/octet_stream/policy.hpp"
#include "caf/net/octet_stream/upper_layer.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <deque>

namespace caf::net::octet_stream {

class CAF_NET_EXPORT transport : public socket_event_layer,
                                 public octet_stream::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using socket_type = stream_socket;

  using connection_handle = stream_socket;

  /// An owning smart pointer type for storing an upper layer object.
  using upper_layer_ptr = std::unique_ptr<octet_stream::upper_layer>;

  /// Bundles various flags into a single block of memory.
  struct flags_t {
    /// Stores whether we left a read handler due to want_write.
    bool wanted_read_from_write_event : 1;

    /// Stores whether we left a write handler due to want_read.
    bool wanted_write_from_read_event : 1;

    /// Stores whether the application has asked to shut down.
    bool shutting_down : 1;
  };

  class policy_impl : public octet_stream::policy {
  public:
    explicit policy_impl(stream_socket fd) : fd(std::move(fd)) {
      // nop
    }

    stream_socket handle() const override;

    ptrdiff_t read(byte_span) override;

    ptrdiff_t write(const_byte_span) override;

    octet_stream::errc last_error(ptrdiff_t) override;

    ptrdiff_t connect() override;

    ptrdiff_t accept() override;

    size_t buffered() const noexcept override;

    stream_socket fd;
  };

  // -- constants --------------------------------------------------------------

  static constexpr size_t default_buf_size = 4 * 1024ul; // 4 KiB

  // -- constructors, destructors, and assignment operators --------------------

  transport(stream_socket fd, upper_layer_ptr up);

  transport(policy* custom, upper_layer_ptr up);

  transport(const transport&) = delete;

  transport& operator=(const transport&) = delete;

  ~transport();

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<transport> make(stream_socket fd, upper_layer_ptr up);

  // -- implementation of octet_stream::lower_layer ----------------------------

  multiplexer& mpx() noexcept override;

  bool can_send_more() const noexcept override;

  void configure_read(receive_policy policy) override;

  void begin_output() override;

  byte_buffer& output_buffer() override;

  bool end_output() override;

  bool is_reading() const noexcept override;

  void write_later() override;

  void shutdown() override;

  void switch_protocol(upper_layer_ptr) override;

  bool switching_protocol() const noexcept override;

  // -- properties -------------------------------------------------------------

  auto& read_buffer() noexcept {
    return read_buf_;
  }

  const auto& read_buffer() const noexcept {
    return read_buf_;
  }

  auto& write_buffer() noexcept {
    return write_buf_;
  }

  const auto& write_buffer() const noexcept {
    return write_buf_;
  }

  auto& upper_layer() noexcept {
    return *up_;
  }

  const auto& upper_layer() const noexcept {
    return *up_;
  }

  policy& active_policy() {
    return *policy_;
  }

  size_t max_consecutive_reads() const noexcept {
    return max_consecutive_reads_;
  }

  void max_consecutive_reads(size_t value) noexcept {
    max_consecutive_reads_ = value;
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(socket_manager* owner) override;

  socket handle() const override;

  void handle_read_event() override;

  void handle_write_event() override;

  void abort(const error& reason) override;

  bool finalized() const noexcept override;

protected:
  // -- utility functions ------------------------------------------------------

  /// Consumes as much data from the buffer as possible.
  void handle_buffered_data();

  /// Calls abort on the upper layer and deregisters the transport from events.
  void fail(const error& reason);

  // -- member variables -------------------------------------------------------

  /// Stores temporary flags.
  flags_t flags_;

  /// Caches the config parameter for limiting max. socket operations.
  size_t max_consecutive_reads_ = defaults::middleman::max_consecutive_reads;

  /// Caches the write buffer size of the socket.
  uint32_t max_write_buf_size_ = 0;

  /// Stores what the user has configured as read threshold.
  uint32_t min_read_size_ = 0;

  /// Stores what the user has configured as max. number of bytes to receive.
  uint32_t max_read_size_ = 0;

  /// Stores how many bytes are currently buffered, i.e., how many bytes from
  /// `read_buf_` are filled with actual data.
  size_t buffered_ = 0;

  /// Stores the offset in `read_buf_` since last calling
  /// `upper_layer_.consume`.
  size_t delta_offset_ = 0;

  /// Caches incoming data.
  byte_buffer read_buf_;

  /// Caches outgoing data.
  byte_buffer write_buf_;

  /// Processes incoming data and generates outgoing data.
  upper_layer_ptr up_;

  /// Configures how we read and write to the socket.
  socket_manager* parent_ = nullptr;

  /// Configures how we read and write to the socket.
  policy* policy_ = nullptr;

  /// Fallback policy.
  union {
    policy_impl default_policy_;
  };

  /// Setting this to non-null informs the transport to replace `up_` with
  /// `next_`.
  upper_layer_ptr next_;
};

} // namespace caf::net::octet_stream
