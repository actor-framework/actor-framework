/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include "caf/actor_system_config.hpp"
#include "caf/byte.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/net/defaults.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/transport_base.hpp"
#include "caf/net/transport_worker.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

template <class Application>
using transport_base_type = transport_base<transport_worker<Application>,
                                           stream_socket, Application, unit_t>;

/// Implements a stream_transport that manages a stream socket.
template <class Application>
class stream_transport : transport_base_type<Application> {
public:
  // -- member types -----------------------------------------------------------

  using application_type = Application;

  using transport_type = stream_transport;

  using worker_type = transport_worker<application_type>;

  using buffer_type = std::vector<byte>;

  using buffer_cache_type = std::vector<buffer_type>;

  // -- constructors, destructors, and assignment operators --------------------

  stream_transport(stream_socket handle, application_type application)
    : transport_base_type<application_type>(handle, std::move(application)) {
    // nop
  }

  // -- member functions -------------------------------------------------------

  error init(endpoint_manager& parent) override {
    // call init function from base class
    transport_base_type<application_type>::init(parent);
  }

  bool handle_read_event(endpoint_manager&) override {
    auto buf = read_buf_.data() + collected_;
    size_t len = read_threshold_ - collected_;
    CAF_LOG_TRACE(CAF_ARG(this->handle_.id) << CAF_ARG(len));
    auto ret = read(handle_, make_span(buf, len));
    // Update state.
    if (auto num_bytes = get_if<size_t>(&ret)) {
      CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id) << CAF_ARG(*num_bytes));
      collected_ += *num_bytes;
      if (collected_ >= read_threshold_) {
        if (auto err = worker_.handle_data(*this, read_buf_)) {
          CAF_LOG_WARNING("handle_data failed:" << CAF_ARG(err));
          return false;
        }
        // TODO: Is this the proper way or would
        // `transport_base_type<Application>::prepare_next_read()` be better?
        this->prepare_next_read();
      }
    } else {
      auto err = get<sec>(ret);
      if (err != sec::unavailable_or_would_block) {
        CAF_LOG_DEBUG("receive failed" << CAF_ARG(err));
        worker_.handle_error(err);
        return false;
      }
    }
    return true;
  }

  bool handle_write_event(endpoint_manager& parent) override {
    // Try to write leftover data.
    write_some();
    // Get new data from parent.
    // TODO: dont read all messages at once - get one by one.
    for (auto msg = parent.next_message(); msg != nullptr;
         msg = parent.next_message()) {
      worker_.write_message(*this, std::move(msg));
    }
    // Write prepared data.
    return write_some();
  }

  void write_packet(unit_t, span<buffer_type*> buffers) override {
    CAF_ASSERT(!buffers.empty());
    if (write_queue_.empty())
      this->manager().register_writing();
    // By convention, the first buffer is a header buffer. Every other buffer is
    // a payload buffer.
    auto i = buffers.begin();
    write_queue_.emplace_back(true, std::move(*(*i++)));
    while (i != buffers.end())
      write_queue_.emplace_back(false, std::move(*(*i++)));
  }

private:
  // -- utility functions ------------------------------------------------------

  bool write_some() {
    CAF_LOG_TRACE(CAF_ARG(handle_.id));
    // Helper function to sort empty buffers back into the right caches.
    auto recycle = [&]() {
      auto& front = write_queue_.front();
      auto& is_header = front.first;
      auto& buf = front.second;
      written_ = 0;
      buf.clear();
      if (is_header) {
        if (header_bufs_.size() < header_bufs_.capacity())
          header_bufs_.emplace_back(std::move(buf));
      } else if (payload_bufs_.size() < payload_bufs_.capacity()) {
        payload_bufs_.emplace_back(std::move(buf));
      }
      write_queue_.pop_front();
    };
    // Write buffers from the write_queue_ for as long as possible.
    while (!write_queue_.empty()) {
      auto& buf = write_queue_.front().second;
      CAF_ASSERT(!buf.empty());
      auto data = buf.data() + written_;
      auto len = buf.size() - written_;
      auto write_ret = write(this->next_layer_, make_span(data, len));
      if (auto num_bytes = get_if<size_t>(&write_ret)) {
        CAF_LOG_DEBUG(CAF_ARG(handle_.id) << CAF_ARG(*num_bytes));
        if (*num_bytes + written_ >= buf.size()) {
          recycle();
          written_ = 0;
        } else {
          written_ += *num_bytes;
          return false;
        }
      } else {
        auto err = get<sec>(write_ret);
        if (err != sec::unavailable_or_would_block) {
          CAF_LOG_DEBUG("send failed" << CAF_ARG(err));
          next_layer_.handle_error(err);
          return false;
        }
        return true;
      }
    }
    return false;
  }

  buffer_cache_type header_bufs_;
  buffer_cache_type payload_bufs_;

  buffer_type read_buf_;
  std::deque<std::pair<bool, buffer_type>> write_queue_;

  // TODO implement retries using this member!
  // size_t max_consecutive_reads_;
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  receive_policy_flag rd_flag_;

  size_t written_;

  endpoint_manager* manager_;
};

} // namespace net
} // namespace caf
