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
#include "caf/net/stream_oriented_layer_ptr.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/sec.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"
#include "caf/tag/io_event_oriented.hpp"
#include "caf/tag/stream_oriented.hpp"

namespace caf::net {

/// Implements a stream_transport that manages a stream socket.
template <class UpperLayer>
class stream_transport {
public:
  // -- member types -----------------------------------------------------------

  using input_tag = tag::io_event_oriented;

  using output_tag = tag::stream_oriented;

  using socket_type = stream_socket;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit stream_transport(Ts&&... xs)
    : upper_layer_(std::forward<Ts>(xs)...) {
    // nop
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
    if (policy.max_size > 0 && max_read_size_ == 0)
      parent->register_reading();
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
  bool handle_read_event(ParentPtr parent) {
    CAF_LOG_TRACE(CAF_ARG2("socket", parent->handle().id));
    auto fail = [this, parent](auto reason) {
      CAF_LOG_DEBUG("read failed" << CAF_ARG(reason));
      parent->abort_reason(std::move(reason));
      auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
      upper_layer_.abort(this_layer_ptr, parent->abort_reason());
      return false;
    };
    if (read_buf_.size() < max_read_size_)
      read_buf_.resize(max_read_size_);
    auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
    static constexpr bool has_after_reading
      = detail::has_after_reading_v<UpperLayer, decltype(this_layer_ptr)>;
    for (size_t i = 0; max_read_size_ > 0 && i < max_consecutive_reads_; ++i) {
      // Calling configure_read(read_policy::stop()) halts receive events.
      if (max_read_size_ == 0) {
        if constexpr (has_after_reading)
          upper_layer_.after_reading(this_layer_ptr);
        return false;
      } else if (offset_ >= max_read_size_) {
        auto old_max = max_read_size_;
        // This may happen if the upper layer changes it receive policy to a
        // smaller max. size than what was already available. In this case, the
        // upper layer must consume bytes before we can receive new data.
        auto bytes = make_span(read_buf_.data(), max_read_size_);
        ptrdiff_t consumed = upper_layer_.consume(this_layer_ptr, bytes, {});
        CAF_LOG_DEBUG(CAF_ARG2("socket", parent->handle().id)
                      << CAF_ARG(consumed));
        if (consumed < 0) {
          upper_layer_.abort(this_layer_ptr,
                             parent->abort_reason_or(caf::sec::runtime_error));
          return false;
        } else if (consumed == 0) {
          // At the very least, the upper layer must accept more data next time.
          if (old_max >= max_read_size_) {
            upper_layer_.abort(this_layer_ptr, parent->abort_reason_or(
                                                 caf::sec::runtime_error,
                                                 "unable to make progress"));
            return false;
          }
        }
        // Try again.
        continue;
      }
      auto rd_buf = make_span(read_buf_.data() + offset_,
                              max_read_size_ - static_cast<size_t>(offset_));
      auto read_res = read(parent->handle(), rd_buf);
      CAF_LOG_DEBUG(CAF_ARG2("socket", parent->handle().id) //
                    << CAF_ARG(max_read_size_) << CAF_ARG(offset_)
                    << CAF_ARG(read_res));
      // Update state.
      if (read_res > 0) {
        offset_ += read_res;
        if (offset_ < min_read_size_)
          continue;
        auto bytes = make_span(read_buf_.data(), offset_);
        auto delta = bytes.subspan(delta_offset_);
        ptrdiff_t consumed = upper_layer_.consume(this_layer_ptr, bytes, delta);
        CAF_LOG_DEBUG(CAF_ARG2("socket", parent->handle().id)
                      << CAF_ARG(consumed));
        if (consumed > 0) {
          // Shift unconsumed bytes to the beginning of the buffer.
          if (consumed < offset_)
            std::copy(read_buf_.begin() + consumed, read_buf_.begin() + offset_,
                      read_buf_.begin());
          offset_ -= consumed;
          delta_offset_ = offset_;
        } else if (consumed < 0) {
          upper_layer_.abort(this_layer_ptr,
                             parent->abort_reason_or(caf::sec::runtime_error,
                                                     "consumed < 0"));
          return false;
        }
        // Our thresholds may have changed if the upper layer called
        // configure_read. Shrink/grow buffer as necessary.
        if (read_buf_.size() != max_read_size_)
          if (offset_ < max_read_size_)
            read_buf_.resize(max_read_size_);
      } else if (read_res < 0) {
        // Try again later on temporary errors such as EWOULDBLOCK and
        // stop reading on the socket on hard errors.
        if (last_socket_error_is_temporary()) {
          if constexpr (has_after_reading)
            upper_layer_.after_reading(this_layer_ptr);
          return true;
        } else {
          return fail(sec::socket_operation_failed);
        }
      } else {
        // read() returns 0 iff the connection was closed.
        return fail(sec::socket_disconnected);
      }
    }
    // Calling configure_read(read_policy::stop()) halts receive events.
    if constexpr (has_after_reading)
      upper_layer_.after_reading(this_layer_ptr);
    return max_read_size_ > 0;
  }

  template <class ParentPtr>
  bool handle_write_event(ParentPtr parent) {
    CAF_LOG_TRACE(CAF_ARG2("socket", parent->handle().id));
    auto fail = [this, parent](sec reason) {
      CAF_LOG_DEBUG("read failed" << CAF_ARG(reason));
      parent->abort_reason(reason);
      auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
      upper_layer_.abort(this_layer_ptr, reason);
      return false;
    };
    // Allow the upper layer to add extra data to the write buffer.
    auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
    if (!upper_layer_.prepare_send(this_layer_ptr)) {
      upper_layer_.abort(this_layer_ptr,
                         parent->abort_reason_or(caf::sec::runtime_error,
                                                 "prepare_send failed"));
      return false;
    }
    if (write_buf_.empty())
      return !upper_layer_.done_sending(this_layer_ptr);
    auto written = write(parent->handle(), write_buf_);
    if (written > 0) {
      write_buf_.erase(write_buf_.begin(), write_buf_.begin() + written);
      return !write_buf_.empty() || !upper_layer_.done_sending(this_layer_ptr);
    } else if (written < 0) {
      // Try again later on temporary errors such as EWOULDBLOCK and
      // stop writing to the socket on hard errors.
      return last_socket_error_is_temporary()
               ? true
               : fail(sec::socket_operation_failed);

    } else {
      // write() returns 0 iff the connection was closed.
      return fail(sec::socket_disconnected);
    }
  }

  template <class ParentPtr>
  void continue_reading(ParentPtr parent) {
    if (max_read_size_ == 0) {
      auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
      upper_layer_.continue_reading(this_layer_ptr);
    }
  }

  template <class ParentPtr>
  void abort(ParentPtr parent, const error& reason) {
    auto this_layer_ptr = make_stream_oriented_layer_ptr(this, parent);
    upper_layer_.abort(this_layer_ptr, reason);
  }

private:
  // Caches the config parameter for limiting max. socket operations.
  uint32_t max_consecutive_reads_ = 0;

  // Caches the write buffer size of the socket.
  uint32_t max_write_buf_size_ = 0;

  // Stores what the user has configured as read threshold.
  uint32_t min_read_size_ = 0;

  // Stores what the user has configured as max. number of bytes to receive.
  uint32_t max_read_size_ = 0;

  // Stores the current offset in `read_buf_`.
  ptrdiff_t offset_ = 0;

  // Stores the offset in `read_buf_` since last calling `upper_layer_.consume`.
  ptrdiff_t delta_offset_ = 0;

  // Caches incoming data.
  byte_buffer read_buf_;

  // Caches outgoing data.
  byte_buffer write_buf_;

  // Processes incoming data and generates outgoing data.
  UpperLayer upper_layer_;
};

} // namespace caf::net
