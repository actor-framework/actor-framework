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

#include "caf/io/network/stream.hpp"

#include <algorithm>

#include "caf/logger.hpp"
#include "caf/defaults.hpp"
#include "caf/config_value.hpp"

#include "caf/io/network/default_multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

stream::stream(default_multiplexer& backend_ref, native_socket sockfd)
    : event_handler(backend_ref, sockfd),
      max_consecutive_reads_(
        get_or(backend().system().config(), "middleman.max-consecutive-reads",
               defaults::middleman::max_consecutive_reads)),
      read_threshold_(1),
      collected_(0),
      ack_writes_(false),
      writing_(false),
      written_(0) {
  configure_read(receive_policy::at_most(1024));
}

void stream::start(stream_manager* mgr) {
  CAF_ASSERT(mgr != nullptr);
  activate(mgr);
}

void stream::activate(stream_manager* mgr) {
  if (!reader_) {
    reader_.reset(mgr);
    event_handler::activate();
    prepare_next_read();
  }
}

void stream::configure_read(receive_policy::config config) {
  rd_flag_ = config.first;
  max_ = config.second;
}

void stream::ack_writes(bool x) {
  ack_writes_ = x;
}

void stream::write(const void* buf, size_t num_bytes) {
  CAF_LOG_TRACE(CAF_ARG(num_bytes));
  auto first = reinterpret_cast<const char*>(buf);
  auto last  = first + num_bytes;
  wr_offline_buf_.insert(wr_offline_buf_.end(), first, last);
}

void stream::flush(const manager_ptr& mgr) {
  CAF_ASSERT(mgr != nullptr);
  CAF_LOG_TRACE(CAF_ARG(wr_offline_buf_.size()));
  if (!wr_offline_buf_.empty() && !writing_) {
    backend().add(operation::write, fd(), this);
    writer_ = mgr;
    writing_ = true;
    prepare_next_write();
  }
}

void stream::stop_reading() {
  CAF_LOG_TRACE("");
  close_read_channel();
  passivate();
}

void stream::removed_from_loop(operation op) {
  CAF_LOG_TRACE(CAF_ARG(op));
  switch (op) {
    case operation::read:  reader_.reset(); break;
    case operation::write: writer_.reset(); break;
    case operation::propagate_error: ; // nop
  }
}

void stream::force_empty_write(const manager_ptr& mgr) {
  if (!writing_) {
    backend().add(operation::write, fd(), this);
    writer_ = mgr;
    writing_ = true;
  }
}

void stream::prepare_next_read() {
  collected_ = 0;
  switch (rd_flag_) {
    case receive_policy_flag::exactly:
      if (rd_buf_.size() != max_)
        rd_buf_.resize(max_);
      read_threshold_ = max_;
      break;
    case receive_policy_flag::at_most:
      if (rd_buf_.size() != max_)
        rd_buf_.resize(max_);
      read_threshold_ = 1;
      break;
    case receive_policy_flag::at_least: {
      // read up to 10% more, but at least allow 100 bytes more
      auto max_size = max_ + std::max<size_t>(100, max_ / 10);
      if (rd_buf_.size() != max_size)
        rd_buf_.resize(max_size);
      read_threshold_ = max_;
      break;
    }
  }
}

void stream::prepare_next_write() {
  CAF_LOG_TRACE(CAF_ARG(wr_buf_.size()) << CAF_ARG(wr_offline_buf_.size()));
  written_ = 0;
  wr_buf_.clear();
  if (wr_offline_buf_.empty()) {
    writing_ = false;
    backend().del(operation::write, fd(), this);
  } else {
    wr_buf_.swap(wr_offline_buf_);
  }
}

bool stream::handle_read_result(rw_state read_result, size_t rb) {
  switch (read_result) {
    case rw_state::failure:
      reader_->io_failure(&backend(), operation::read);
      passivate();
      return false;
    case rw_state::indeterminate:
      return false;
    case rw_state::success:
      if (rb == 0)
        return false;
      collected_ += rb;
      if (collected_ >= read_threshold_) {
        auto res = reader_->consume(&backend(), rd_buf_.data(),
                                    collected_);
        prepare_next_read();
        if (!res) {
          passivate();
          return false;
        }
      }
      break;
  }
  return true;
}

void stream::handle_write_result(rw_state write_result, size_t wb) {
  switch (write_result) {
    case rw_state::failure:
      writer_->io_failure(&backend(), operation::write);
      backend().del(operation::write, fd(), this);
      break;
    case rw_state::indeterminate:
      prepare_next_write();
      break;
    case rw_state::success:
      written_ += wb;
      CAF_ASSERT(written_ <= wr_buf_.size());
      auto remaining = wr_buf_.size() - written_;
      if (ack_writes_)
        writer_->data_transferred(&backend(), wb,
                                  remaining + wr_offline_buf_.size());
      // prepare next send (or stop sending)
      if (remaining == 0)
        prepare_next_write();
      break;
  }
}

void stream::handle_error_propagation() {
  if (reader_)
    reader_->io_failure(&backend(), operation::read);
  if (writer_)
    writer_->io_failure(&backend(), operation::write);
}

} // namespace network
} // namespace io
} // namespace caf
