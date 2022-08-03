// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <deque>

#include "caf/byte_buffer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/socket_event_layer.hpp"
#include "caf/net/stream_oriented.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport_error.hpp"

namespace caf::net {

class CAF_NET_EXPORT stream_transport : public socket_event_layer,
                                        public stream_oriented::lower_layer {
public:
  // -- member types -----------------------------------------------------------

  using socket_type = stream_socket;

  using connection_handle = stream_socket;

  /// An owning smart pointer type for storing an upper layer object.
  using upper_layer_ptr = std::unique_ptr<stream_oriented::upper_layer>;

  class policy {
  public:
    enum class ec {
      /// Indicates that the transport should try again later.
      temporary,
      /// Indicates that the transport must read data before trying again.
      want_read,
      /// Indicates that the transport must write data before trying again.
      want_write,
      /// Indicates that the transport cannot resume this operation.
      permanent,
    };

    virtual ~policy();

    /// Reads data from the socket into the buffer.
    virtual ptrdiff_t read(stream_socket x, byte_span buf) = 0;

    /// Writes data from the buffer to the socket.
    virtual ptrdiff_t write(stream_socket x, const_byte_span buf) = 0;

    /// Returns the last socket error on this thread.
    virtual stream_transport_error last_error(stream_socket, ptrdiff_t) = 0;

    /// Checks whether connecting a non-blocking socket was successful.
    virtual ptrdiff_t connect(stream_socket x) = 0;

    /// Convenience function that always returns 1. Exists to make writing code
    /// against multiple policies easier by providing the same interface.
    virtual ptrdiff_t accept(stream_socket) = 0;

    /// Returns the number of bytes that are buffered internally and available
    /// for immediate read.
    virtual size_t buffered() const noexcept = 0;
  };

  class default_policy : public policy {
  public:
    ptrdiff_t read(stream_socket x, byte_span buf) override;

    ptrdiff_t write(stream_socket x, const_byte_span buf) override;

    stream_transport_error last_error(stream_socket, ptrdiff_t) override;

    ptrdiff_t connect(stream_socket x) override;

    ptrdiff_t accept(stream_socket) override;

    size_t buffered() const noexcept override;
  };

  /// Bundles various flags into a single block of memory.
  struct flags_t {
    /// Stores whether we left a read handler due to want_write.
    bool wanted_read_from_write_event : 1;

    /// Stores whether we left a write handler due to want_read.
    bool wanted_write_from_read_event : 1;

    /// Stores whether the application has asked to shut down.
    bool shutting_down : 1;
  };

  // -- constants --------------------------------------------------------------

  static constexpr size_t default_buf_size = 4 * 1024; // 4 KiB

  // -- constructors, destructors, and assignment operators --------------------

  stream_transport(stream_socket fd, upper_layer_ptr up);

  stream_transport(stream_socket fd, upper_layer_ptr up, policy* custom);

  stream_transport(const stream_transport&) = delete;

  stream_transport& operator=(const stream_transport&) = delete;

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<stream_transport> make(stream_socket fd,
                                                upper_layer_ptr up);

  // -- implementation of stream_oriented::lower_layer -------------------------

  bool can_send_more() const noexcept override;

  void configure_read(receive_policy policy) override;

  void begin_output() override;

  byte_buffer& output_buffer() override;

  bool end_output() override;

  bool is_reading() const noexcept override;

  void write_later() override;

  void shutdown() override;

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

  // -- implementation of socket_event_layer -----------------------------------

  error start(socket_manager* owner, const settings& config) override;

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

  /// The socket file descriptor.
  stream_socket fd_;

  /// Stores temporary flags.
  flags_t flags_;

  /// Caches the config parameter for limiting max. socket operations.
  uint32_t max_consecutive_reads_ = 0;

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
  default_policy default_policy_;

  // TODO: add [[no_unique_address]] to default_policy_ when switching to C++20.
};

/// @relates stream_transport
CAF_NET_EXPORT std::string to_string(stream_transport::policy::ec);

/// @relates stream_transport
CAF_NET_EXPORT bool from_string(std::string_view,
                                stream_transport::policy::ec&);

/// @relates stream_transport
CAF_NET_EXPORT bool from_integer(int, stream_transport::policy::ec&);

/// @relates stream_transport
template <class Inspector>
bool inspect(Inspector& f, stream_transport::policy::ec& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::net
