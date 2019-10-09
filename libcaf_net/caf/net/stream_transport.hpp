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

  using transport_type = stream_transport;

  using worker_type = transport_worker<application_type>;

  // -- constructors, destructors, and assignment operators --------------------

  stream_transport(stream_socket handle, application_type application)
    : worker_(std::move(application)),
      handle_(handle),
      // max_consecutive_reads_(0),
      read_threshold_(1024),
      collected_(0),
      max_(1024),
      rd_flag_(net::receive_policy_flag::exactly),
      written_(0),
      manager_(nullptr) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  stream_socket handle() const noexcept {
    return handle_;
  }

  actor_system& system() {
    return manager().system();
  }

  application_type& application() {
    return worker_.application();
  }

  transport_type& transport() {
    return *this;
  }

  endpoint_manager& manager() {
    return *manager_;
  }

  // -- member functions -------------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    manager_ = &parent;
    if (auto err = worker_.init(*this))
      return err;
    return none;
  }

  template <class Parent>
  bool handle_read_event(Parent&) {
    auto buf = read_buf_.data() + collected_;
    size_t len = read_threshold_ - collected_;
    CAF_LOG_TRACE(CAF_ARG(handle_.id) << CAF_ARG(len));
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
        prepare_next_read();
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

  template <class Parent>
  bool handle_write_event(Parent& parent) {
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

  template <class Parent>
  void resolve(Parent&, const uri& locator, const actor& listener) {
    worker_.resolve(*this, locator.path(), listener);
  }

  template <class Parent>
  void new_proxy(Parent&, const node_id& peer, actor_id id) {
    worker_.new_proxy(*this, peer, id);
  }

  template <class Parent>
  void local_actor_down(Parent&, const node_id& peer, actor_id id,
                        error reason) {
    worker_.local_actor_down(*this, peer, id, std::move(reason));
  }

  template <class Parent>
  void timeout(Parent&, atom_value value, uint64_t id) {
    worker_.timeout(*this, value, id);
  }

  template <class... Ts>
  void set_timeout(uint64_t, Ts&&...) {
    // nop
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

  void write_packet(span<const byte> header, span<const byte> payload,
                    typename worker_type::id_type) {
    if (write_buf_.empty())
      manager().register_writing();
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
      if (err != sec::unavailable_or_would_block) {
        CAF_LOG_DEBUG("send failed" << CAF_ARG(err));
        worker_.handle_error(err);
        return false;
      }
    }
    return true;
  }

  worker_type worker_;
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

  endpoint_manager* manager_;
};

} // namespace net
} // namespace caf
