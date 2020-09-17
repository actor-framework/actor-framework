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

#include <deque>

#include "caf/byte_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/sec.hpp"
#include "caf/settings.hpp"
#include "caf/span.hpp"
#include "caf/tag/stream_oriented.hpp"

namespace caf::net {

/// Implements a stream_transport that manages a stream socket.
template <class UpperLayer>
class stream_transport {
public:
  // -- member types -----------------------------------------------------------

  using output_tag = tag::stream_oriented;

  using socket_type = stream_socket;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  stream_transport(Ts&&... xs) : upper_layer_(std::forward<Ts>(xs)...) {
    // nop
  }

  virtual ~stream_transport() {
    // nop
  }

  // -- interface for the upper layer ------------------------------------------

  template <class Parent>
  class access {
  public:
    access(Parent* parent, stream_transport* transport)
      : parent_(parent), transport_(transport) {
      // nop
    }

    void begin_output() {
      if (transport_->write_buf_.empty())
        parent_->register_writing();
    }

    byte_buffer& output_buffer() {
      return transport_->write_buf_;
    }

    constexpr void end_output() {
      // nop
    }

    bool can_send_more() const noexcept {
      return output_buffer().size() < transport_->max_write_buf_size_;
    }

    void abort_reason(error reason) {
      return parent_->abort_reason(std::move(reason));
    }

    void configure_read(receive_policy policy) {
      if (policy.max_size > 0 && transport_->max_read_size_ == 0)
        parent_->register_reading();
      transport_->min_read_size_ = policy.min_size;
      transport_->max_read_size_ = policy.max_size;
      transport_->read_buf_.resize(policy.max_size);
    }

  private:
    Parent* parent_;
    stream_transport* transport_;
  };

  template <class Parent>
  friend class access;

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

  template <class Parent>
  error init(socket_manager* owner, Parent& parent, const settings& config) {
    namespace mm = defaults::middleman;
    auto default_max_reads = static_cast<uint32_t>(mm::max_consecutive_reads);
    max_consecutive_reads_ = get_or(
      config, "caf.middleman.max-consecutive-reads", default_max_reads);
    // TODO: Tests fail, since nodelay can not be set on unix domain sockets.
    // Maybe we can check for test runs using `if constexpr`?
    /*    if (auto err = nodelay(socket_cast<stream_socket>(parent.handle()),
                               true)) {
          CAF_LOG_ERROR("nodelay failed: " << err);
          return err;
        }*/
    if (auto socket_buf_size
        = send_buffer_size(parent.template handle<network_socket>())) {
      max_write_buf_size_ = *socket_buf_size;
      CAF_ASSERT(max_write_buf_size_ > 0);
      write_buf_.reserve(max_write_buf_size_ * 2);
    } else {
      CAF_LOG_ERROR("send_buffer_size: " << socket_buf_size.error());
      return std::move(socket_buf_size.error());
    }
    access<Parent> this_layer{&parent, this};
    return upper_layer_.init(owner, this_layer, config);
  }

  // -- event callbacks --------------------------------------------------------

  template <class Parent>
  bool handle_read_event(Parent& parent) {
    CAF_LOG_TRACE(CAF_ARG2("handle", parent.handle().id));
    auto fail = [this, &parent](auto reason) {
      CAF_LOG_DEBUG("read failed" << CAF_ARG(reason));
      parent.abort_reason(std::move(reason));
      access<Parent> this_layer{&parent, this};
      upper_layer_.abort(this_layer, parent.abort_reason());
      return false;
    };
    access<Parent> this_layer{&parent, this};
    for (size_t i = 0; max_read_size_ > 0 && i < max_consecutive_reads_; ++i) {
      CAF_ASSERT(max_read_size_ > read_size_);
      auto buf = read_buf_.data() + read_size_;
      size_t len = max_read_size_ - read_size_;
      CAF_LOG_DEBUG(CAF_ARG2("missing", len));
      auto num_bytes = read(parent.template handle<socket_type>(),
                            make_span(buf, len));
      CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG2("handle", parent.handle().id)
                                 << CAF_ARG(num_bytes));
      // Update state.
      if (num_bytes > 0) {
        read_size_ += num_bytes;
        if (read_size_ < min_read_size_)
          continue;
        auto delta = make_span(read_buf_.data() + delta_offset_,
                               read_size_ - delta_offset_);
        auto consumed = upper_layer_.consume(this_layer, make_span(read_buf_),
                                             delta);
        if (consumed > 0) {
          read_buf_.erase(read_buf_.begin(), read_buf_.begin() + consumed);
          read_size_ -= consumed;
        } else if (consumed < 0) {
          upper_layer_.abort(this_layer,
                             parent.abort_reason_or(caf::sec::runtime_error));
          return false;
        }
        delta_offset_ = read_size_;
      } else if (num_bytes < 0) {
        // Try again later on temporary errors such as EWOULDBLOCK and
        // stop reading on the socket on hard errors.
        return last_socket_error_is_temporary()
                 ? true
                 : fail(sec::socket_operation_failed);

      } else {
        // read() returns 0 iff the connection was closed.
        return fail(sec::socket_disconnected);
      }
    }
    // Calling configure_read(read_policy::stop()) halts receive events.
    return max_read_size_ > 0;
  }

  template <class Parent>
  bool handle_write_event(Parent& parent) {
    CAF_LOG_TRACE(CAF_ARG2("handle", parent.handle().id));
    auto fail = [this, &parent](sec reason) {
      CAF_LOG_DEBUG("read failed" << CAF_ARG(reason));
      parent.abort_reason(reason);
      access<Parent> this_layer{&parent, this};
      upper_layer_.abort(this_layer, reason);
      return false;
    };
    // Allow the upper layer to add extra data to the write buffer.
    access<Parent> this_layer{&parent, this};
    if (!upper_layer_.prepare_send(this_layer)) {
      upper_layer_.abort(this_layer,
                         parent.abort_reason_or(caf::sec::runtime_error));
      return false;
    }
    if (write_buf_.empty())
      return !upper_layer_.done_sending(this_layer);
    auto written = write(parent.template handle<socket_type>(), write_buf_);
    if (written > 0) {
      write_buf_.erase(write_buf_.begin(), write_buf_.begin() + written);
      return !write_buf_.empty() || !upper_layer_.done_sending(this_layer);
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

  template <class Parent>
  void abort(Parent& parent, const error& reason) {
    access<Parent> this_layer{&parent, this};
    upper_layer_.abort(this_layer, reason);
  }

private:
  uint32_t max_consecutive_reads_ = 0;
  uint32_t max_write_buf_size_ = 0;
  uint32_t min_read_size_ = 0;
  uint32_t max_read_size_ = 0;
  ptrdiff_t read_size_ = 0;
  ptrdiff_t delta_offset_ = 0;
  byte_buffer read_buf_;
  byte_buffer write_buf_;
  UpperLayer upper_layer_;
};

} // namespace caf::net
