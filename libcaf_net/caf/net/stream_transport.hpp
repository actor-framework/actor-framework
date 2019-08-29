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

#include "caf/byte.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/logger.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/transport_worker.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

/// Implements a stream_transport that manages a stream socket.
template <class Application>
class stream_transport {
public:
  // -- member types -----------------------------------------------------------

  using application_type = Application;

  // -- constructors, destructors, and assignment operators --------------------

  stream_transport(stream_socket handle, application_type application)
    : worker_(std::move(application)),
      handle_(handle),
      // max_consecutive_reads_(0),
      read_threshold_(1024),
      collected_(0),
      max_(1024),
      rd_flag_(net::receive_policy_flag::exactly),
      written_(0) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  stream_socket handle() const noexcept {
    return handle_;
  }

  // -- member functions -------------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    worker_.init(parent);
    parent.mask_add(operation::read);
    return none;
  }

  template <class Parent>
  bool handle_read_event(Parent& parent) {
    auto buf = read_buf_.data() + collected_;
    size_t len = read_threshold_ - collected_;
    CAF_LOG_TRACE(CAF_ARG(handle_.id) << CAF_ARG(len));
    auto ret = read(handle_, make_span(buf, len));
    // Update state.
    if (auto num_bytes = get_if<size_t>(&ret)) {
      CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id) << CAF_ARG(*num_bytes));
      collected_ += *num_bytes;
      if (collected_ >= read_threshold_) {
        auto decorator = make_write_packet_decorator(*this, parent);
        worker_.handle_data(decorator, read_buf_);
        prepare_next_read();
      }
      return true;
    } else {
      auto err = get<sec>(ret);
      CAF_LOG_DEBUG("receive failed" << CAF_ARG(err));
      worker_.handle_error(err);
      return false;
    }
  }

  template <class Parent>
  bool handle_write_event(Parent& parent) {
    // Try to write leftover data.
    write_some();
    // Get new data from parent.
    // TODO: dont read all messages at once - get one by one.
    for (auto msg = parent.next_message(); msg != nullptr;
         msg = parent.next_message()) {
      auto decorator = make_write_packet_decorator(*this, parent);
      worker_.write_message(decorator, std::move(msg));
    }
    // Write prepared data.
    return write_some();
  }

  template <class Parent>
  void resolve(Parent& parent, const std::string& path, actor listener) {
    worker_.resolve(parent, path, listener);
  }

  template <class... Ts>
  uint64_t set_timeout(uint64_t, Ts&&...) {
    // nop
  }

  template <class Parent>
  void timeout(Parent& parent, atom_value value, uint64_t id) {
    worker_.timeout(parent, value, id);
  }

  void handle_error(sec code) {
    worker_.handle_error(code);
  }

  void prepare_next_read() {
    read_buf_.clear();
    collected_ = 0;
    // This cast does nothing, but prevents a weird compiler error on GCC
    // <= 4.9.
    // TODO: remove cast when dropping support for GCC 4.9.
    switch (static_cast<net::receive_policy_flag>(rd_flag_)) {
      case net::receive_policy_flag::exactly:
        if (read_buf_.size() != max_)
          read_buf_.resize(max_);
        read_threshold_ = max_;
        break;
      case net::receive_policy_flag::at_most:
        if (read_buf_.size() != max_)
          read_buf_.resize(max_);
        read_threshold_ = 1;
        break;
      case net::receive_policy_flag::at_least: {
        // read up to 10% more, but at least allow 100 bytes more
        auto max_size = max_ + std::max<size_t>(100, max_ / 10);
        if (read_buf_.size() != max_size)
          read_buf_.resize(max_size);
        read_threshold_ = max_;
        break;
      }
    }
  }

  void configure_read(receive_policy::config cfg) {
    rd_flag_ = cfg.first;
    max_ = cfg.second;
    prepare_next_read();
  }

  template <class Parent>
  void write_packet(Parent&, span<const byte> header, span<const byte> payload,
                    unit_t) {
    write_buf_.insert(write_buf_.end(), header.begin(), header.end());
    write_buf_.insert(write_buf_.end(), payload.begin(), payload.end());
  }

private:
  // -- private member functions -----------------------------------------------

  bool write_some() {
    if (write_buf_.empty())
      return false;
    auto len = write_buf_.size() - written_;
    auto buf = write_buf_.data() + written_;
    CAF_LOG_TRACE(CAF_ARG(handle_.id) << CAF_ARG(len));
    auto ret = write(handle_, make_span(buf, len));
    if (auto num_bytes = get_if<size_t>(&ret)) {
      CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id) << CAF_ARG(*num_bytes));
      // Update state.
      written_ += *num_bytes;
      if (written_ >= write_buf_.size()) {
        written_ = 0;
        write_buf_.clear();
        return false;
      }
    } else {
      auto err = get<sec>(ret);
      CAF_LOG_DEBUG("send failed" << CAF_ARG(err));
      worker_.handle_error(err);
      return false;
    }
    return true;
  }

  transport_worker<application_type> worker_;
  stream_socket handle_;

  std::vector<byte> read_buf_;
  std::vector<byte> write_buf_;

  // TODO implement retries using this member!
  // size_t max_consecutive_reads_;
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  receive_policy_flag rd_flag_;

  size_t written_;
};

} // namespace net
} // namespace caf
