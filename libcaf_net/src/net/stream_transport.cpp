// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/stream_transport.hpp"

#include "caf/defaults.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"

namespace caf::net {

// -- member types -------------------------------------------------------------

stream_transport::policy::~policy() {
  // nop
}

ptrdiff_t stream_transport::default_policy::read(stream_socket x,
                                                 byte_span buf) {
  return net::read(x, buf);
}

ptrdiff_t stream_transport::default_policy::write(stream_socket x,
                                                  const_byte_span buf) {
  return net::write(x, buf);
}

stream_transport_error
stream_transport::default_policy::last_error(stream_socket, ptrdiff_t) {
  return last_socket_error_is_temporary() ? stream_transport_error::temporary
                                          : stream_transport_error::permanent;
}

ptrdiff_t stream_transport::default_policy::connect(stream_socket x) {
  // A connection is established if the OS reports a socket as ready for read
  // or write and if there is no error on the socket.
  return net::probe(x) ? 1 : -1;
}

ptrdiff_t stream_transport::default_policy::accept(stream_socket) {
  return 1;
}

size_t stream_transport::default_policy::buffered() const noexcept {
  return 0;
}

// -- constructors, destructors, and assignment operators ----------------------

stream_transport::stream_transport(stream_socket fd, upper_layer_ptr up)
  : fd_(fd), up_(std::move(up)), policy_(&default_policy_) {
  memset(&flags_, 0, sizeof(flags_t));
}

stream_transport::stream_transport(stream_socket fd, upper_layer_ptr up,
                                   policy* policy)
  : fd_(fd), up_(std::move(up)), policy_(policy) {
  memset(&flags_, 0, sizeof(flags_t));
}

// -- factories ----------------------------------------------------------------

std::unique_ptr<stream_transport> stream_transport::make(stream_socket fd,
                                                         upper_layer_ptr up) {
  return std::make_unique<stream_transport>(fd, std::move(up));
}

// -- implementation of stream_oriented::lower_layer ---------------------------

bool stream_transport::can_send_more() const noexcept {
  return write_buf_.size() < max_write_buf_size_;
}

void stream_transport::configure_read(receive_policy rd) {
  auto restarting = rd.max_size > 0 && max_read_size_ == 0;
  min_read_size_ = rd.min_size;
  max_read_size_ = rd.max_size;
  if (restarting && !parent_->is_reading()) {
    if (buffered_ > 0 && buffered_ >= min_read_size_) {
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

void stream_transport::begin_output() {
  if (write_buf_.empty())
    parent_->register_writing();
}

byte_buffer& stream_transport::output_buffer() {
  return write_buf_;
}

bool stream_transport::end_output() {
  return true;
}

bool stream_transport::is_reading() const noexcept {
  return max_read_size_ > 0;
}

void stream_transport::write_later() {
  parent_->register_writing();
}

void stream_transport::shutdown() {
  if (write_buf_.empty()) {
    parent_->shutdown();
  } else {
    configure_read(receive_policy::stop());
    parent_->deregister_reading();
    flags_.shutting_down = true;
  }
}

// -- implementation of transport ----------------------------------------------

error stream_transport::start(socket_manager* owner, const settings& config) {
  parent_ = owner;
  namespace mm = defaults::middleman;
  auto default_max_reads = static_cast<uint32_t>(mm::max_consecutive_reads);
  max_consecutive_reads_ = get_or(config, "caf.middleman.max-consecutive-reads",
                                  default_max_reads);
  // if (auto err = nodelay(fd_, true)) {
  //   CAF_LOG_ERROR("nodelay failed: " << err);
  //   return err;
  // }
  if (auto socket_buf_size = send_buffer_size(fd_)) {
    max_write_buf_size_ = *socket_buf_size;
    CAF_ASSERT(max_write_buf_size_ > 0);
    write_buf_.reserve(max_write_buf_size_ * 2);
  } else {
    CAF_LOG_ERROR("send_buffer_size: " << socket_buf_size.error());
    return std::move(socket_buf_size.error());
  }
  return up_->start(this, config);
}

socket stream_transport::handle() const {
  return fd_;
}

void stream_transport::handle_read_event() {
  CAF_LOG_TRACE(CAF_ARG2("socket", fd_.id));
  // Resume a write operation if the transport waited for the socket to be
  // readable from the last call to handle_write_event.
  if (flags_.wanted_read_from_write_event) {
    flags_.wanted_read_from_write_event = false;
    // The subsequent call to handle_write_event expects a writing manager.
    parent_->register_writing();
    handle_write_event();
    if (!parent_->is_reading()) {
      // The call to handle_write_event deregisters the manager from reading in
      // case of an error. So we need to double-check that flag here.
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
  auto rd = policy_->read(fd_, make_span(read_buf_.data() + buffered_,
                                         read_buf_.size() - buffered_));
  // Stop if we failed to get more data.
  if (rd < 0) {
    switch (policy_->last_error(fd_, rd)) {
      case stream_transport_error::temporary:
      case stream_transport_error::want_read:
        // Try again later.
        return;
      case stream_transport_error::want_write:
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
    auto rd2 = policy_->read(fd_, make_span(read_buf_.data() + buffered_,
                                            policy_buffered));
    if (rd2 != static_cast<ptrdiff_t>(policy_buffered)) {
      CAF_LOG_ERROR("failed to read buffered data from the policy");
      return fail(make_error(sec::socket_operation_failed));
    }
    buffered_ += static_cast<size_t>(rd2);
  }
  // Read buffered data and then allow other sockets to run.
  handle_buffered_data();
}

void stream_transport::handle_buffered_data() {
  CAF_LOG_TRACE(CAF_ARG(buffered_));
  // Loop until we have drained the buffer as much as we can.
  CAF_ASSERT(min_read_size_ <= max_read_size_);
  while (max_read_size_ > 0 && buffered_ >= min_read_size_) {
    auto n = std::min(buffered_, size_t{max_read_size_});
    auto bytes = make_span(read_buf_.data(), n);
    auto delta = bytes.subspan(delta_offset_);
    auto consumed = up_->consume(bytes, delta);
    if (consumed < 0) {
      // Negative values indicate that the application encountered an
      // unrecoverable error.
      up_->abort(make_error(caf::sec::runtime_error, "consumed < 0"));
      parent_->deregister();
      return;
    } else if (static_cast<size_t>(consumed) > n) {
      // Must not happen. An application cannot handle more data then we pass
      // to it.
      up_->abort(make_error(sec::logic_error, "consumed > buffer.size"));
      parent_->deregister();
      return;
    } else {
      // Shove the unread bytes to the beginning of the buffer and continue
      // to the next loop iteration.
      auto del = static_cast<size_t>(consumed);
      auto prev = buffered_;
      buffered_ -= del;
      delta_offset_ = static_cast<ptrdiff_t>(n - del);
      if (buffered_ > 0) {
        auto new_begin = read_buf_.begin() + del;
        auto new_end = read_buf_.begin() + prev;
        std::copy(new_begin, new_end, read_buf_.begin());
      }
    }
  }
  if (max_read_size_ == 0)
    parent_->deregister_reading();
}

void stream_transport::fail(const error& reason) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  up_->abort(reason);
  up_.reset();
  parent_->deregister();
  parent_->shutdown();
}

void stream_transport::handle_write_event() {
  CAF_LOG_TRACE(CAF_ARG2("socket", fd_.id));
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
  auto write_res = policy_->write(fd_, write_buf_);
  if (write_res > 0) {
    write_buf_.erase(write_buf_.begin(), write_buf_.begin() + write_res);
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
    switch (policy_->last_error(fd_, write_res)) {
      case stream_transport_error::temporary:
      case stream_transport_error::want_write:
        return;
      case stream_transport_error::want_read:
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

void stream_transport::abort(const error& reason) {
  up_->abort(reason);
  flags_.shutting_down = true;
}

bool stream_transport::finalized() const noexcept {
  return write_buf_.empty();
}

// -- free functions -----------------------------------------------------------

std::string to_string(stream_transport::policy::ec code) {
  switch (code) {
    case stream_transport::policy::ec::temporary:
      return "temporary";
    case stream_transport::policy::ec::want_read:
      return "want_read";
    case stream_transport::policy::ec::want_write:
      return "want_write";
    case stream_transport::policy::ec::permanent:
      return "permanent";
    default:
      return "invalid";
  }
}

bool from_string(std::string_view str, stream_transport::policy::ec& code) {
  if (str == "temporary") {
    code = stream_transport::policy::ec::temporary;
    return true;
  } else if (str == "want_read") {
    code = stream_transport::policy::ec::want_read;
    return true;
  } else if (str == "want_write") {
    code = stream_transport::policy::ec::want_write;
    return true;
  } else if (str == "permanent") {
    code = stream_transport::policy::ec::permanent;
    return true;
  } else {
    return false;
  }
}

bool from_integer(int value, stream_transport::policy::ec& code) {
  if (value >= 0 && value < 4) {
    code = static_cast<stream_transport::policy::ec>(value);
    return true;
  } else {
    return false;
  }
}

} // namespace caf::net
