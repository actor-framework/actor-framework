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

#include "caf/io/network/datagram_handler.hpp"

#include <algorithm>

#include "caf/logger.hpp"
#include "caf/defaults.hpp"
#include "caf/config_value.hpp"

#include "caf/io/network/default_multiplexer.hpp"

namespace {

constexpr size_t receive_buffer_size = std::numeric_limits<uint16_t>::max();

} // namespace anonymous

namespace caf {
namespace io {
namespace network {

datagram_handler::datagram_handler(default_multiplexer& backend_ref,
                                   native_socket sockfd)
    : event_handler(backend_ref, sockfd),
      max_consecutive_reads_(
        get_or(backend().system().config(), "middleman.max-consecutive-reads",
               defaults::middleman::max_consecutive_reads)),
      max_datagram_size_(receive_buffer_size),
      rd_buf_(receive_buffer_size),
      send_buffer_size_(0) {
  allow_udp_connreset(sockfd, false);
  auto es = send_buffer_size(sockfd);
  if (!es)
    CAF_LOG_ERROR("cannot determine socket buffer size");
  else
    send_buffer_size_ = *es;
}

void datagram_handler::start(datagram_manager* mgr) {
  CAF_LOG_TRACE(CAF_ARG2("fd", fd()));
  CAF_ASSERT(mgr != nullptr);
  activate(mgr);
}

void datagram_handler::activate(datagram_manager* mgr) {
  if (!reader_) {
    reader_.reset(mgr);
    event_handler::activate();
    prepare_next_read();
  }
}

void datagram_handler::write(datagram_handle hdl, const void* buf,
                             size_t num_bytes) {
  wr_offline_buf_.emplace_back();
  wr_offline_buf_.back().first = hdl;
  auto cbuf = reinterpret_cast<const char*>(buf);
  wr_offline_buf_.back().second.assign(cbuf,
                                       cbuf + static_cast<ptrdiff_t>(num_bytes));
}

void datagram_handler::flush(const manager_ptr& mgr) {
  CAF_ASSERT(mgr != nullptr);
  CAF_LOG_TRACE(CAF_ARG(wr_offline_buf_.size()));
  if (!wr_offline_buf_.empty() && !state_.writing) {
    backend().add(operation::write, fd(), this);
    writer_ = mgr;
    state_.writing = true;
    prepare_next_write();
  }
}

std::unordered_map<datagram_handle, ip_endpoint>& datagram_handler::endpoints() {
  return ep_by_hdl_;
}

const std::unordered_map<datagram_handle, ip_endpoint>&
datagram_handler::endpoints() const {
  return ep_by_hdl_;
}

void datagram_handler::add_endpoint(datagram_handle hdl, const ip_endpoint& ep,
                                    const manager_ptr mgr) {
  auto itr = hdl_by_ep_.find(ep);
  if (itr == hdl_by_ep_.end()) {
    hdl_by_ep_[ep] = hdl;
    ep_by_hdl_[hdl] = ep;
    writer_ = mgr;
  } else if (!writer_) {
    writer_ = mgr;
  } else {
    CAF_LOG_ERROR("cannot assign a second servant to the endpoint "
                  << to_string(ep));
    abort();
  }
}

void datagram_handler::remove_endpoint(datagram_handle hdl) {
  CAF_LOG_TRACE(CAF_ARG(hdl));
  auto itr = ep_by_hdl_.find(hdl);
  if (itr != ep_by_hdl_.end()) {
    hdl_by_ep_.erase(itr->second);
    ep_by_hdl_.erase(itr);
  }
}

void datagram_handler::removed_from_loop(operation op) {
  switch (op) {
    case operation::read: reader_.reset(); break;
    case operation::write: writer_.reset(); break;
    case operation::propagate_error: ; // nop
  };
}

void datagram_handler::graceful_shutdown() {
  CAF_LOG_TRACE(CAF_ARG2("fd", fd_));
  // Ignore repeated calls.
  if (state_.shutting_down)
    return;
  state_.shutting_down = true;
  // Stop reading right away.
  passivate();
  // UDP is connectionless. Hence, there's no graceful way to shutdown
  // anything. This handler gets destroyed automatically once it no longer is
  // registered for reading or writing.
}

void datagram_handler::prepare_next_read() {
  CAF_LOG_TRACE(CAF_ARG(wr_buf_.second.size())
                << CAF_ARG(wr_offline_buf_.size()));
  rd_buf_.resize(max_datagram_size_);
}

void datagram_handler::prepare_next_write() {
  CAF_LOG_TRACE(CAF_ARG(wr_offline_buf_.size()));
  wr_buf_.second.clear();
  if (wr_offline_buf_.empty()) {
    state_.writing = false;
    backend().del(operation::write, fd(), this);
  } else {
    wr_buf_.swap(wr_offline_buf_.front());
    wr_offline_buf_.pop_front();
  }
}

bool datagram_handler::handle_read_result(bool read_result) {
  if (!read_result) {
    reader_->io_failure(&backend(), operation::read);
    passivate();
    return false;
  }
  if (num_bytes_ > 0) {
    rd_buf_.resize(num_bytes_);
    auto itr = hdl_by_ep_.find(sender_);
    bool consumed = false;
    if (itr == hdl_by_ep_.end())
      consumed = reader_->new_endpoint(rd_buf_);
    else
      consumed = reader_->consume(&backend(), itr->second, rd_buf_);
    prepare_next_read();
    if (!consumed) {
      passivate();
      return false;
    }
  }
  return true;
}

void datagram_handler::handle_write_result(bool write_result, datagram_handle id,
                                           std::vector<char>& buf, size_t wb) {
  if (!write_result) {
    writer_->io_failure(&backend(), operation::write);
    backend().del(operation::write, fd(), this);
  } else if (wb > 0) {
    CAF_ASSERT(wb == buf.size());
    if (state_.ack_writes)
      writer_->datagram_sent(&backend(), id, wb, std::move(buf));
    prepare_next_write();
  } else {
    if (writer_)
      writer_->io_failure(&backend(), operation::write);
  }
}

void datagram_handler::handle_error() {
  if (reader_)
    reader_->io_failure(&backend(), operation::read);
  if (writer_)
    writer_->io_failure(&backend(), operation::write);
  // backend will delete this handler anyway,
  // no need to call backend().del() here
}

} // namespace network
} // namespace io
} // namespace caf
