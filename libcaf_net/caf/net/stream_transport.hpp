// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <deque>

#include "caf/byte_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/has_after_reading.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_oriented_layer_ptr.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport_error.hpp"
#include "caf/sec.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"
#include "caf/tag/io_event_oriented.hpp"
#include "caf/tag/stream_oriented.hpp"

namespace caf::net {

/// Configures a stream transport with default socket operations.
struct default_stream_transport_policy {
public:
  /// Reads data from the socket into the buffer.
  static ptrdiff_t read(stream_socket x, span<byte> buf) {
    return net::read(x, buf);
  }

  /// Writes data from the buffer to the socket.
  static ptrdiff_t write(stream_socket x, span<const byte> buf) {
    return net::write(x, buf);
  }

  /// Returns the last socket error on this thread.
  static stream_transport_error last_error(stream_socket, ptrdiff_t) {
    return last_socket_error_is_temporary() ? stream_transport_error::temporary
                                            : stream_transport_error::permanent;
  }

  /// Returns the number of bytes that are buffered internally and that
  /// available for immediate read.
  static constexpr size_t buffered() {
    return 0;
  }
};

/// Implements a stream_transport that manages a stream socket.
template <class Policy, class UpperLayer>
class stream_transport_base {
public:
  // -- member types -----------------------------------------------------------

  using input_tag = tag::io_event_oriented;

  using output_tag = tag::stream_oriented;

  using socket_type = stream_socket;

  using read_result = typename socket_manager::read_result;

  using write_result = typename socket_manager::write_result;

  /// Bundles various flags into a single block of memory.
  struct flags_t {
    /// Stores whether we left a read handler due to want_write.
    bool wanted_read_from_write_event : 1;

    /// Stores whether we left a write handler due to want_read.
    bool wanted_write_from_read_event : 1;
  };

  // -- constants --------------------------------------------------------------

  static constexpr size_t default_buf_size = 4 * 1024; // 4 KiB

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit stream_transport_base(Policy policy, Ts&&... xs)
    : upper_layer_(std::forward<Ts>(xs)...), policy_(std::move(policy)) {
    memset(&flags, 0, sizeof(flags_t));
    read_buf_.resize(default_buf_size);
  }

  // -- interface for stream_oriented_layer_ptr --------------------------------

  template <class ParentPtr>
  bool can_send_more(ParentPtr) const noexcept {
    return write_buf_.size() < max_write_buf_size_;
  }

  template <class ParentPtr>
  static socket_type handle(ParentPtr parent) noexcept {
    return parent->handle();
  }

  template <class ParentPtr>
  void begin_output(ParentPtr parent) {
    if (write_buf_.empty())
      parent->register_writing();
  }

  template <class ParentPtr>
  byte_buffer& output_buffer(ParentPtr) {
    return write_buf_;
  }

  template <class ParentPtr>
  static constexpr void end_output(ParentPtr) {
    // nop
  }

  template <class ParentPtr>
  static void abort_reason(ParentPtr parent, error reason) {
    return parent->abort_reason(std::move(reason));
  }

  template <class ParentPtr>
  static const error& abort_reason(ParentPtr parent) {
    return parent->abort_reason();
  }

  template <class ParentPtr>
  void configure_read(ParentPtr parent, receive_policy policy) {
    if (policy.max_size > 0 && max_read_size_ == 0) {
      parent->register_reading();
    }
    min_read_size_ = policy.min_size;
    max_read_size_ = policy.max_size;
  }

  template <class ParentPtr>
  bool stopped(ParentPtr) const noexcept {
    return max_read_size_ == 0;
  }

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
    return upper_layer_;
  }

  const auto& upper_layer() const noexcept {
    return upper_layer_;
  }

  // -- initialization ---------------------------------------------------------

  template <class ParentPtr>
  error init(socket_manager* owner, ParentPtr parent, const settings& config) {
    namespace mm = defaults::middleman;
    auto default_max_reads = static_cast<uint32_t>(mm::max_consecutive_reads);
    max_consecutive_reads_ = get_or(config,
                                    "caf.middleman.max-consecutive-reads",
                                    default_max_reads);
    auto sock = parent->handle();
    if constexpr (std::is_base_of<tcp_stream_socket, decltype(sock)>::value) {
      if (auto err = nodelay(sock, true)) {
        CAF_LOG_ERROR("nodelay failed: " << err);
        return err;
      }
    }
    if (auto socket_buf_size = send_buffer_size(parent->handle())) {
      max_write_buf_size_ = *socket_buf_size;
      CAF_ASSERT(max_write_buf_size_ > 0);
      write_buf_.reserve(max_write_buf_size_ * 2);
    } else {
      CAF_LOG_ERROR("send_buffer_size: " << socket_buf_size.error());
      return std::move(socket_buf_size.error());
    }
    auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
    return upper_layer_.init(owner, this_layer_ptr, config);
  }

  // -- event callbacks --------------------------------------------------------

  template <class ParentPtr>
  read_result handle_read_event(ParentPtr parent) {
    CAF_LOG_TRACE(CAF_ARG2("socket", parent->handle().id));
    // Pointer for passing "this layer" to the next one down the chain.
    auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
    // Convenience lambda for failing the application.
    auto fail = [this, &parent, &this_layer_ptr](auto reason) {
      CAF_LOG_DEBUG("read failed" << CAF_ARG(reason));
      parent->abort_reason(std::move(reason));
      upper_layer_.abort(this_layer_ptr, parent->abort_reason());
      return read_result::stop;
    };
    // Resume a write operation if the transport waited for the socket to be
    // readable from the last call to handle_write_event.
    if (flags.wanted_read_from_write_event) {
      flags.wanted_read_from_write_event = false;
      switch (handle_write_event(parent)) {
        case write_result::want_read:
          CAF_ASSERT(flags.wanted_read_from_write_event);
          return read_result::again;
        case write_result::handover:
          return read_result::handover;
        case write_result::again:
          parent->register_writing();
          break;
        default:
          break;
      }
    }
    // Before returning from the event handler, we always call after_reading for
    // clients that request this callback.
    using detail::make_scope_guard;
    auto after_reading_guard = make_scope_guard([this, &this_layer_ptr] { //
      after_reading(this_layer_ptr);
    });
    // Make sure the buffer is large enough.
    if (read_buf_.size() < max_read_size_)
      read_buf_.resize(max_read_size_);
    // Fill up our buffer.
    auto rd = policy_.read(parent->handle(),
                           make_span(read_buf_.data() + buffered_,
                                     read_buf_.size() - buffered_));
    // Stop if we failed to get more data.
    if (rd < 0) {
      switch (policy_.last_error(parent->handle(), rd)) {
        case stream_transport_error::temporary:
        case stream_transport_error::want_read:
          return read_result::again;
        case stream_transport_error::want_write:
          flags.wanted_write_from_read_event = true;
          return read_result::want_write;
        default:
          return fail(sec::socket_operation_failed);
      }
    } else if (rd == 0) {
      return fail(sec::socket_disconnected);
    }
    // Make sure we actually have all data currently available to us and the
    // policy is not holding on to some bytes. This may happen when using
    // OpenSSL or any other transport policy operating on blocks.
    buffered_ += static_cast<size_t>(rd);
    if (auto policy_buffered = policy_.buffered(); policy_buffered > 0) {
      if (auto n = buffered_ + policy_buffered; n > read_buf_.size())
        read_buf_.resize(n);
      auto rd2 = policy_.read(parent->handle(),
                              make_span(read_buf_.data() + buffered_,
                                        policy_buffered));
      if (rd2 != static_cast<ptrdiff_t>(policy_buffered)) {
        CAF_LOG_ERROR("failed to read buffered data from the policy");
        return fail(sec::socket_operation_failed);
      }
      buffered_ += static_cast<size_t>(rd2);
    }
    // Try to make progress on the application and return control to the
    // multiplexer to allow other sockets to run.
    return handle_buffered_data(parent);
  }

  template <class ParentPtr>
  read_result handle_buffered_data(ParentPtr parent) {
    CAF_LOG_TRACE(CAF_ARG(buffered_));
    // Pointer for passing "this layer" to the next one down the chain.
    auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
    // Convenience lambda for failing the application.
    auto fail = [this, &parent, &this_layer_ptr](auto reason) {
      CAF_LOG_DEBUG("read failed" << CAF_ARG(reason));
      parent->abort_reason(std::move(reason));
      upper_layer_.abort(this_layer_ptr, parent->abort_reason());
      return read_result::stop;
    };
    // Loop until we have drained the buffer as much as we can.
    CAF_ASSERT(min_read_size_ <= max_read_size_);
    while (max_read_size_ > 0 && buffered_ >= min_read_size_) {
      auto old_max_read_size = max_read_size_;
      auto n = std::min(buffered_, size_t{max_read_size_});
      auto bytes = make_span(read_buf_.data(), n);
      auto delta = bytes.subspan(delta_offset_);
      auto consumed = upper_layer_.consume(this_layer_ptr, bytes, delta);
      if (consumed < 0) {
        // Negative values indicate that the application encountered an
        // unrecoverable error.
        upper_layer_.abort(this_layer_ptr,
                           parent->abort_reason_or(caf::sec::runtime_error,
                                                   "consumed < 0"));
        return read_result::stop;
      } else if (consumed == 0) {
        // Returning 0 means that the application wants more data. Note:
        // max_read_size_ may have changed if the application realized it
        // requires more data to parse the input. It may of course only increase
        // the max_read_size_ in this case, everything else makes no sense.
        delta_offset_ = static_cast<ptrdiff_t>(n);
        if (n == max_read_size_ || max_read_size_ < old_max_read_size) {
          CAF_LOG_ERROR("application failed to make progress");
          return fail(sec::runtime_error);
        } else if (n == buffered_) {
          // Either the application has increased max_read_size_ or we
          // did not reach max_read_size_ the first time. In both cases, we
          // cannot proceed without receiving more data.
          return read_result::again;
        }
      } else if (static_cast<size_t>(consumed) > n) {
        // Must not happen. An application cannot handle more data then we pass
        // to it.
        upper_layer_.abort(this_layer_ptr,
                           parent->abort_reason_or(caf::sec::runtime_error,
                                                   "consumed > buffer.size"));
        return read_result::stop;
      } else {
        // Shove the unread bytes to the beginning of the buffer and continue
        // to the next loop iteration.
        auto del = static_cast<size_t>(consumed);
        auto prev = buffered_;
        buffered_ -= del;
        delta_offset_ = 0;
        if (buffered_ > 0) {
          auto new_begin = read_buf_.begin() + del;
          auto new_end = read_buf_.begin() + prev;
          std::copy(new_begin, new_end, read_buf_.begin());
        }
      }
    }
    return max_read_size_ > 0 ? read_result::again : read_result::stop;
  }

  template <class ParentPtr>
  write_result handle_write_event(ParentPtr parent) {
    CAF_LOG_TRACE(CAF_ARG2("socket", parent->handle().id));
    auto fail = [this, parent](sec reason) {
      CAF_LOG_DEBUG("read failed" << CAF_ARG(reason));
      parent->abort_reason(reason);
      auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
      upper_layer_.abort(this_layer_ptr, reason);
      return write_result::stop;
    };
    // Resume a read operation if the transport waited for the socket to be
    // writable from the last call to handle_read_event.
    if (flags.wanted_write_from_read_event) {
      flags.wanted_write_from_read_event = false;
      switch (handle_read_event(parent)) {
        case read_result::want_write:
          CAF_ASSERT(flags.wanted_write_from_read_event);
          return write_result::again;
        case read_result::handover:
          return write_result::handover;
        case read_result::again:
          parent->register_reading();
          break;
        default:
          break;
      }
      // Fall though and see if we also have something to write.
    }
    // Allow the upper layer to add extra data to the write buffer.
    auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
    if (!upper_layer_.prepare_send(this_layer_ptr)) {
      upper_layer_.abort(this_layer_ptr,
                         parent->abort_reason_or(caf::sec::runtime_error,
                                                 "prepare_send failed"));
      return write_result::stop;
    }
    if (write_buf_.empty())
      return !upper_layer_.done_sending(this_layer_ptr) ? write_result::again
                                                        : write_result::stop;
    auto write_res = policy_.write(parent->handle(), write_buf_);
    if (write_res > 0) {
      write_buf_.erase(write_buf_.begin(), write_buf_.begin() + write_res);
      return !write_buf_.empty() || !upper_layer_.done_sending(this_layer_ptr)
               ? write_result::again
               : write_result::stop;
    } else if (write_res < 0) {
      // Try again later on temporary errors such as EWOULDBLOCK and
      // stop writing to the socket on hard errors.
      switch (policy_.last_error(parent->handle(), write_res)) {
        case stream_transport_error::temporary:
        case stream_transport_error::want_write:
          return write_result::again;
        case stream_transport_error::want_read:
          flags.wanted_read_from_write_event = true;
          return write_result::want_read;
        default:
          return fail(sec::socket_operation_failed);
      }
    } else {
      // write() returns 0 if the connection was closed.
      return fail(sec::socket_disconnected);
    }
  }

  template <class ParentPtr>
  read_result handle_continue_reading(ParentPtr parent) {
    if (max_read_size_ == 0) {
      auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
      upper_layer_.continue_reading(this_layer_ptr);
    }
    return handle_buffered_data(parent);
  }

  template <class ParentPtr>
  write_result handle_continue_writing(ParentPtr) {
    // TODO: consider whether we need another callback for the upper layer.
    //       For now, we always return `again`, which triggers the write
    //       handler later.
    return write_result::again;
  }

  template <class ParentPtr>
  void abort(ParentPtr parent, const error& reason) {
    auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
    upper_layer_.abort(this_layer_ptr, reason);
  }

private:
  template <class ThisLayerPtr>
  void after_reading([[maybe_unused]] ThisLayerPtr& this_layer_ptr) {
    if constexpr (detail::has_after_reading_v<UpperLayer, ThisLayerPtr>)
      upper_layer_.after_reading(this_layer_ptr);
  }

  flags_t flags;

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
  UpperLayer upper_layer_;

  /// Configures how we read and write to the socket.
  Policy policy_;
};

/// Implements a stream_transport that manages a stream socket.
template <class UpperLayer>
class stream_transport
  : public stream_transport_base<default_stream_transport_policy, UpperLayer> {
public:
  // -- member types -----------------------------------------------------------

  using super
    = stream_transport_base<default_stream_transport_policy, UpperLayer>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit stream_transport(Ts&&... xs)
    : super(default_stream_transport_policy{}, std::forward<Ts>(xs)...) {
    // nop
  }
};

} // namespace caf::net
