// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/network/stream.hpp"

#include "caf/io/network/default_multiplexer.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/config_value.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/assert.hpp"
#include "caf/log/io.hpp"

#include <algorithm>

namespace caf::io::network {

stream::stream(default_multiplexer& backend_ref, native_socket sockfd)
  : event_handler(backend_ref, sockfd),
    max_consecutive_reads_(get_or(backend().system().config(),
                                  "caf.middleman.max-consecutive-reads",
                                  defaults::middleman::max_consecutive_reads)),
    read_threshold_(1),
    collected_(0),
    written_(0),
    wr_op_backoff_(false) {
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
  state_.rd_flag = to_integer(config.first);
  max_ = config.second;
}

void stream::write(const void* buf, size_t num_bytes) {
  auto lg = log::io::trace("num_bytes = {}", num_bytes);
  auto first = reinterpret_cast<const std::byte*>(buf);
  auto last = first + num_bytes;
  wr_offline_buf_.insert(wr_offline_buf_.end(), first, last);
}

void stream::flush(const manager_ptr& mgr) {
  CAF_ASSERT(mgr != nullptr);
  auto lg = log::io::trace("wr_offline_buf_.size = {}", wr_offline_buf_.size());
  if (!wr_offline_buf_.empty() && !state_.writing && !wr_op_backoff_) {
    backend().add(operation::write, fd(), this);
    writer_ = mgr;
    state_.writing = true;
    prepare_next_write();
  }
}

void stream::removed_from_loop(operation op) {
  auto lg = log::io::trace("fd = {}, op = {}", fd_, op);
  switch (op) {
    case operation::read:
      reader_.reset();
      break;
    case operation::write:
      writer_.reset();
      break;
    case operation::propagate_error:; // nop
  }
}

void stream::graceful_shutdown() {
  auto lg = log::io::trace("fd = {}", fd_);
  // Ignore repeated calls.
  if (state_.shutting_down)
    return;
  state_.shutting_down = true;
  // Initiate graceful shutdown unless we have still data to send.
  if (!state_.writing)
    send_fin();
  // Otherwise, send_fin() gets called after draining the send buffer.
}

void stream::force_empty_write(const manager_ptr& mgr) {
  if (!state_.writing) {
    backend().add(operation::write, fd(), this);
    writer_ = mgr;
    state_.writing = true;
  }
}

void stream::prepare_next_read() {
  collected_ = 0;
  switch (static_cast<receive_policy_flag>(state_.rd_flag)) {
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
  auto lg = log::io::trace("wr_buf_.size = {}, wr_offline_buf_.size = {}",
                           wr_buf_.size(), wr_offline_buf_.size());
  written_ = 0;
  wr_buf_.clear();
  if (wr_offline_buf_.empty() || wr_op_backoff_) {
    state_.writing = false;
    backend().del(operation::write, fd(), this);
    if (state_.shutting_down)
      send_fin();
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
      // Recover previous pending write if it is the first successful read after
      // want_read was reported.
      if (wr_op_backoff_) {
        CAF_ASSERT(reader_ != nullptr);
        backend().add(operation::write, fd(), this);
        writer_ = reader_;
        state_.writing = true;
        wr_op_backoff_ = false;
      }
      [[fallthrough]];
    case rw_state::want_read:
      if (rb == 0)
        return false;
      collected_ += rb;
      if (collected_ >= read_threshold_) {
        auto res = reader_->consume(&backend(), rd_buf_.data(), collected_);
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
    case rw_state::want_read:
      // If the write operation returns want_read, we need to suspend writing to
      // the socket until the next successful read. Otherwise, we may cause
      // spinning and high CPU usage.
      backend().del(operation::write, fd(), this);
      wr_op_backoff_ = true;
      if (wb == 0)
        break;
      [[fallthrough]];
    case rw_state::success:
      written_ += wb;
      CAF_ASSERT(written_ <= wr_buf_.size());
      auto remaining = wr_buf_.size() - written_;
      if (state_.ack_writes)
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

void stream::send_fin() {
  auto lg = log::io::trace("fd = {}", fd_);
  // Shutting down the write channel will cause TCP to send FIN for the
  // graceful shutdown sequence. The peer then closes its connection as well
  // and we will notice this by getting 0 as return value of recv without error
  // (connection closed).
  shutdown_write(fd_);
}

} // namespace caf::io::network
