// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/octet_stream/transport.hpp"

#include "caf/net/octet_stream/errc.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/defaults.hpp"
#include "caf/detail/assert.hpp"
#include "caf/expected.hpp"
#include "caf/log/net.hpp"

namespace caf::net::octet_stream {

namespace {

class policy_impl : public octet_stream::policy {
public:
  explicit policy_impl(stream_socket fd) : fd(std::move(fd)) {
    // nop
  }

  stream_socket handle() const override {
    return fd;
  }

  ptrdiff_t read(byte_span buf) override {
    return net::read(fd, buf);
  }

  ptrdiff_t write(const_byte_span buf) override {
    return net::write(fd, buf);
  }

  errc last_error(ptrdiff_t) override {
    return last_socket_error_is_temporary() ? errc::temporary : errc::permanent;
  }

  ptrdiff_t connect() override {
    // A connection is established if the OS reports a socket as ready for read
    // or write and if there is no error on the socket.
    return probe(fd) ? 1 : -1;
  }

  ptrdiff_t accept() override {
    return 1;
  }

  size_t buffered() const noexcept override {
    return 0;
  }

  stream_socket fd;
};

class transport_impl : public transport {
public:
  /// Bundles various flags into a single block of memory.
  struct flags_t {
    /// Stores whether we left a read handler due to want_write.
    bool wanted_read_from_write_event : 1;

    /// Stores whether we left a write handler due to want_read.
    bool wanted_write_from_read_event : 1;

    /// Stores whether the application has asked to shut down.
    bool shutting_down : 1;
  };

  // -- constructors, destructors, and assignment operators --------------------

  transport_impl(stream_socket fd, upper_layer_ptr up) : up_(std::move(up)) {
    memset(&flags_, 0, sizeof(flags_t));
    policy_ = std::make_unique<policy_impl>(fd);
  }

  transport_impl(std::unique_ptr<policy> policy, upper_layer_ptr up)
    : up_(std::move(up)), policy_(std::move(policy)) {
    memset(&flags_, 0, sizeof(flags_t));
  }

  transport_impl(const transport_impl&) = delete;

  transport_impl& operator=(const transport_impl&) = delete;

  ~transport_impl() {
    // nop
  }

  // -- implementation of octet_stream::lower_layer ----------------------------

  socket_manager* manager() noexcept override {
    return parent_;
  }

  bool can_send_more() const noexcept override {
    return write_buf_.size() < max_write_buf_size_;
  }

  void configure_read(receive_policy rd) override {
    auto restarting = rd.max_size > 0 && max_read_size_ == 0;
    min_read_size_ = rd.min_size;
    max_read_size_ = rd.max_size;
    if (restarting && !parent_->is_reading()) {
      if (buffered_ > 0 && buffered_ >= min_read_size_
          && delta_offset_ < buffered_) {
        // We can already make progress with the data we have. Hence, we need
        // schedule a call to read from our buffer before we can wait for
        // additional data from the peer.
        parent_->schedule_fn([this] {
          parent_->register_reading();
          handle_buffered_data();
        });
      } else {
        // Simply ask for more data.
        parent_->register_reading();
      }
    } else if (max_read_size_ == 0) {
      parent_->deregister_reading();
    }
  }

  void begin_output() override {
    if (write_buf_.empty())
      parent_->register_writing();
  }

  byte_buffer& output_buffer() override {
    return write_buf_;
  }

  bool end_output() override {
    return true;
  }

  bool is_reading() const noexcept override {
    return max_read_size_ > 0;
  }

  void write_later() override {
    parent_->register_writing();
  }

  void shutdown() override {
    if (write_buf_.empty()) {
      parent_->shutdown();
    } else {
      configure_read(receive_policy::stop());
      parent_->deregister_reading();
      flags_.shutting_down = true;
    }
  }

  void switch_protocol(upper_layer_ptr next) override {
    next_ = std::move(next);
  }

  bool switching_protocol() const noexcept override {
    return next_ != nullptr;
  }

  // -- properties -------------------------------------------------------------

  policy& active_policy() override {
    return *policy_;
  }

  size_t max_consecutive_reads() const noexcept override {
    return max_consecutive_reads_;
  }

  void max_consecutive_reads(size_t value) noexcept override {
    max_consecutive_reads_ = value;
  }

  // -- implementation of socket_event_layer -----------------------------------

  error start(socket_manager* owner) override {
    parent_ = owner;
    if (auto socket_buf_size = send_buffer_size(policy_->handle())) {
      max_write_buf_size_ = *socket_buf_size;
      CAF_ASSERT(max_write_buf_size_ > 0);
      write_buf_.reserve(max_write_buf_size_ * 2ul);
    } else {
      log::net::error("send_buffer_size: {}", socket_buf_size.error());
      return std::move(socket_buf_size.error());
    }
    return up_->start(this);
  }

  socket handle() const override {
    return policy_->handle();
  }

  void handle_read_event() override {
    auto lg = log::net::trace("socket = {}", handle());
    // Resume a write operation if the transport waited for the socket to be
    // readable from the last call to handle_write_event.
    if (flags_.wanted_read_from_write_event) {
      flags_.wanted_read_from_write_event = false;
      // The subsequent call to handle_write_event expects a writing manager.
      parent_->register_writing();
      handle_write_event();
      if (!parent_->is_reading()) {
        // The call to handle_write_event deregisters the manager from reading
        // in case of an error. So we need to double-check that flag here.
        return;
      }
      // Check if we have actually some reading to do.
      if (max_read_size_ == 0) {
        if (!flags_.wanted_read_from_write_event)
          parent_->deregister_reading();
        return;
      }
    }
    // Make sure our read buffer is large enough.
    if (read_buf_.size() < max_read_size_)
      read_buf_.resize(max_read_size_);
    // Fill up our buffer.
    auto rd = policy_->read(
      make_span(read_buf_.data() + buffered_, read_buf_.size() - buffered_));
    // Stop if we failed to get more data.
    if (rd < 0) {
      switch (policy_->last_error(rd)) {
        case errc::temporary:
        case errc::want_read:
          // Try again later.
          return;
        case errc::want_write:
          // Wait for writable socket and then call handle_read_event again.
          flags_.wanted_write_from_read_event = true;
          parent_->register_writing();
          parent_->deregister_reading();
          return;
        default:
          return fail(make_error(sec::socket_operation_failed));
      }
    } else if (rd == 0) {
      return fail(make_error(sec::socket_disconnected));
    }
    // Make sure we actually have all data currently available to us and the
    // policy is not holding on to some bytes. This may happen when using
    // OpenSSL or any other transport policy that operates on blocks.
    buffered_ += static_cast<size_t>(rd);
    if (auto policy_buffered = policy_->buffered(); policy_buffered > 0) {
      if (auto n = buffered_ + policy_buffered; n > read_buf_.size())
        read_buf_.resize(n);
      auto rd2 = policy_->read(
        make_span(read_buf_.data() + buffered_, policy_buffered));
      if (rd2 != static_cast<ptrdiff_t>(policy_buffered)) {
        log::net::error("failed to read buffered data from the policy");
        return fail(make_error(sec::socket_operation_failed));
      }
      buffered_ += static_cast<size_t>(rd2);
    }
    // Read buffered data and then allow other sockets to run.
    handle_buffered_data();
  }

  void handle_write_event() override {
    auto lg = log::net::trace("socket = {}", handle());
    // Resume a read operation if the transport waited for the socket to be
    // writable from the last call to handle_read_event.
    if (flags_.wanted_write_from_read_event) {
      flags_.wanted_write_from_read_event = false;
      // The subsequent call to handle_read_event expects a writing manager.
      parent_->register_writing();
      handle_read_event();
      if (!parent_->is_writing()) {
        // The call to handle_read_event deregisters the manager from writing in
        // case of an error. So we need to double-check that flag here.
        return;
      }
    }
    // When shutting down, we flush our buffer and then shut down the manager.
    if (flags_.shutting_down) {
      if (write_buf_.empty()) {
        parent_->shutdown();
        return;
      }
    } else if (can_send_more()) {
      // Allow the upper layer to add extra data to the write buffer.
      up_->prepare_send();
    }
    auto write_res = policy_->write(write_buf_);
    if (write_res > 0) {
      write_buf_.erase(write_buf_.begin(), write_buf_.begin() + write_res);
      up_->written(static_cast<size_t>(write_res));
      if (write_buf_.empty() && up_->done_sending()) {
        if (!flags_.shutting_down) {
          parent_->deregister_writing();
        } else {
          parent_->shutdown();
        }
      }
      return;
    } else if (write_res < 0) {
      // Try again later on temporary errors such as EWOULDBLOCK and
      // stop writing to the socket on hard errors.
      switch (policy_->last_error(write_res)) {
        case errc::temporary:
        case errc::want_write:
          return;
        case errc::want_read:
          flags_.wanted_read_from_write_event = true;
          parent_->register_reading();
          parent_->deregister_writing();
          return;
        default:
          return fail(make_error(sec::socket_operation_failed));
      }
    } else {
      // write() returns 0 if the connection was closed.
      return fail(make_error(sec::socket_disconnected));
    }
  }

  void abort(const error& reason) override {
    up_->abort(reason);
    flags_.shutting_down = true;
  }

  bool finalized() const noexcept override {
    return write_buf_.empty();
  }

protected:
  // -- utility functions ------------------------------------------------------

  /// Consumes as much data from the buffer as possible.
  void handle_buffered_data() {
    auto lg = log::net::trace("buffered_ = {}", buffered_);
    // Loop until we have drained the buffer as much as we can.
    CAF_ASSERT(min_read_size_ <= max_read_size_);
    auto switch_to_next_protocol = [this] {
      CAF_ASSERT(next_);
      // Switch to the new protocol and initialize it.
      configure_read(receive_policy::stop());
      up_.reset(next_.release());
      if (auto err = up_->start(this)) {
        up_.reset();
        parent_->deregister();
        parent_->shutdown();
        return false;
      }
      return true;
    };
    while (parent_->is_reading() && max_read_size_ > 0
           && buffered_ >= min_read_size_) {
      auto n = std::min(buffered_, size_t{max_read_size_});
      auto bytes = make_span(read_buf_.data(), n);
      auto delta = bytes.subspan(delta_offset_);
      auto consumed = up_->consume(bytes, delta);
      if (consumed < 0) {
        // Negative values indicate that the application wants to close the
        // socket. We still make sure to send any pending data before closing.
        up_->abort(make_error(caf::sec::runtime_error, "consumed < 0"));
        parent_->deregister_reading();
        return;
      } else if (static_cast<size_t>(consumed) > n) {
        // Must not happen. An application cannot handle more data then we pass
        // to it.
        up_->abort(make_error(sec::logic_error, "consumed > buffer.size"));
        parent_->deregister_reading();
        return;
      } else if (consumed == 0) {
        if (next_) {
          // When switching protocol, the new layer has never seen the data, so
          // we might just re-invoke the same data again.
          if (!switch_to_next_protocol())
            return;
        } else {
          // See whether the next iteration would change what we pass to the
          // application (max_read_size_ may have changed). Otherwise, we'll try
          // again later.
          delta_offset_ = static_cast<ptrdiff_t>(n);
          if (n == std::min(buffered_, size_t{max_read_size_}))
            return;
        }
      } else {
        if (next_ && !switch_to_next_protocol())
          return;
        // Shove the unread bytes to the beginning of the buffer and continue
        // to the next loop iteration.
        auto prev = static_cast<ptrdiff_t>(buffered_);
        buffered_ -= static_cast<size_t>(consumed);
        delta_offset_ = 0;
        if (buffered_ > 0)
          std::copy(read_buf_.begin() + consumed, read_buf_.begin() + prev,
                    read_buf_.begin());
      }
    }
  }

  /// Calls abort on the upper layer and deregisters the transport from events.
  void fail(const error& reason) {
    auto lg = log::net::trace("reason = {}", reason);
    up_->abort(reason);
    up_.reset();
    parent_->deregister();
    parent_->shutdown();
  }

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

  /// Points to the associated socket manager below.
  socket_manager* parent_ = nullptr;

  /// Configures how we read and write to the socket.
  std::unique_ptr<policy> policy_;

  /// Setting this to non-null informs the transport to replace `up_` with
  /// `next_`.
  upper_layer_ptr next_;
};

} // namespace

// -- factories ----------------------------------------------------------------

std::unique_ptr<transport> transport::make(stream_socket fd,
                                           upper_layer_ptr up) {
  return std::make_unique<transport_impl>(fd, std::move(up));
}

std::unique_ptr<transport> transport::make(std::unique_ptr<policy> policy,
                                           upper_layer_ptr up) {
  return std::make_unique<transport_impl>(std::move(policy), std::move(up));
}

} // namespace caf::net::octet_stream
