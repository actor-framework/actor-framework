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
#include <vector>

#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/net/defaults.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/transport_base.hpp"
#include "caf/net/transport_worker.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

namespace caf::net {

template <class Application>
using stream_transport_base = transport_base<
  stream_transport<Application>, transport_worker<Application>, stream_socket,
  Application, unit_t>;

/// Implements a stream_transport that manages a stream socket.
template <class Application>
class stream_transport : public stream_transport_base<Application> {
public:
  // -- member types -----------------------------------------------------------

  using application_type = Application;

  using worker_type = transport_worker<application_type>;

  using super = stream_transport_base<application_type>;

  using id_type = typename super::id_type;

  using buffer_type = typename super::buffer_type;

  using write_queue_type = std::deque<std::pair<bool, buffer_type>>;

  // -- constructors, destructors, and assignment operators --------------------

  stream_transport(stream_socket handle, application_type application)
    : super(handle, std::move(application)),
      written_(0),
      read_threshold_(1024),
      collected_(0),
      max_(1024),
      rd_flag_(net::receive_policy_flag::exactly) {
    CAF_ASSERT(handle != invalid_socket);
    nodelay(handle, true);
  }

  // -- member functions -------------------------------------------------------

  bool handle_read_event(endpoint_manager&) override {
    auto buf = this->read_buf_.data() + this->collected_;
    size_t len = this->read_threshold_ - this->collected_;
    CAF_LOG_TRACE(CAF_ARG2("handle", this->handle().id)
                  << CAF_ARG2("missing", len));
    auto ret = read(this->handle_, make_span(buf, len));
    // Update state.
    if (auto num_bytes = get_if<size_t>(&ret)) {
      CAF_LOG_DEBUG(CAF_ARG(len)
                    << CAF_ARG(this->handle_.id) << CAF_ARG(*num_bytes));
      this->collected_ += *num_bytes;
      if (this->collected_ >= this->read_threshold_) {
        if (auto err = this->next_layer_.handle_data(*this,
                                                     make_span(
                                                       this->read_buf_))) {
          CAF_LOG_ERROR("handle_data failed: " << CAF_ARG(err));
          return false;
        }
        this->prepare_next_read();
      }
    } else {
      auto err = get<sec>(ret);
      if (err != sec::unavailable_or_would_block) {
        CAF_LOG_DEBUG("read failed" << CAF_ARG(err));
        this->next_layer_.handle_error(err);
        return false;
      }
    }
    return true;
  }

  bool handle_write_event(endpoint_manager& manager) override {
    CAF_LOG_TRACE(CAF_ARG2("handle", this->handle_.id)
                  << CAF_ARG2("queue-size", write_queue_.size()));
    auto fetch_next_message = [&] {
      if (auto msg = manager.next_message()) {
        this->next_layer_.write_message(*this, std::move(msg));
        return true;
      }
      return false;
    };
    do {
      if (auto err = write_some())
        return err == sec::unavailable_or_would_block;
    } while (fetch_next_message());
    return !write_queue_.empty();
  }

  void write_packet(id_type, span<buffer_type*> buffers) override {
    CAF_LOG_TRACE("");
    CAF_ASSERT(!buffers.empty());
    if (this->write_queue_.empty())
      this->manager().register_writing();
    // By convention, the first buffer is a header buffer. Every other buffer is
    // a payload buffer.
    auto i = buffers.begin();
    this->write_queue_.emplace_back(true, std::move(*(*i++)));
    while (i != buffers.end())
      this->write_queue_.emplace_back(false, std::move(*(*i++)));
  }

  void configure_read(receive_policy::config cfg) override {
    rd_flag_ = cfg.first;
    max_ = cfg.second;
    prepare_next_read();
  }

private:
  // -- utility functions ------------------------------------------------------

  void prepare_next_read() {
    collected_ = 0;
    switch (rd_flag_) {
      case net::receive_policy_flag::exactly:
        if (this->read_buf_.size() != max_)
          this->read_buf_.resize(max_);
        read_threshold_ = max_;
        break;
      case net::receive_policy_flag::at_most:
        if (this->read_buf_.size() != max_)
          this->read_buf_.resize(max_);
        read_threshold_ = 1;
        break;
      case net::receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto max_size = max_ + std::max<size_t>(100, max_ / 10);
        if (this->read_buf_.size() != max_size)
          this->read_buf_.resize(max_size);
        read_threshold_ = max_;
        break;
      }
    }
  }

  error write_some() {
    // Helper function to sort empty buffers back into the right caches.
    auto recycle = [&]() {
      auto& front = this->write_queue_.front();
      auto& is_header = front.first;
      auto& buf = front.second;
      written_ = 0;
      buf.clear();
      if (is_header) {
        if (this->header_bufs_.size() < this->header_bufs_.capacity())
          this->header_bufs_.emplace_back(std::move(buf));
      } else if (this->payload_bufs_.size() < this->payload_bufs_.capacity()) {
        this->payload_bufs_.emplace_back(std::move(buf));
      }
      write_queue_.pop_front();
    };
    // Write buffers from the write_queue_ for as long as possible.
    while (!write_queue_.empty()) {
      auto& buf = write_queue_.front().second;
      CAF_ASSERT(!buf.empty());
      auto data = buf.data() + written_;
      auto len = buf.size() - written_;
      auto write_ret = write(this->handle(), make_span(data, len));
      if (auto num_bytes = get_if<size_t>(&write_ret)) {
        CAF_LOG_DEBUG(CAF_ARG(this->handle_.id) << CAF_ARG(*num_bytes));
        written_ += *num_bytes;
        if (written_ >= buf.size()) {
          recycle();
          written_ = 0;
        }
      } else {
        auto err = get<sec>(write_ret);
        if (err != sec::unavailable_or_would_block) {
          CAF_LOG_DEBUG("send failed" << CAF_ARG(err));
          this->next_layer_.handle_error(err);
        }
        return err;
      }
    }
    return none;
  }

  write_queue_type write_queue_;
  size_t written_;
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  receive_policy_flag rd_flag_;
  // TODO implement retries using this member!
  // size_t max_consecutive_reads_;
};

} // namespace caf::net
