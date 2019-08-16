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
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace policy {

/// Implements a scribe policy that manages a stream socket.
class scribe {
public:
  explicit scribe(net::stream_socket handle);

  net::stream_socket handle() const noexcept {
    return handle_;
  }

  template <class Parent>
  error init(Parent& parent) {
    parent.application().init(parent);
    parent.mask_add(net::operation::read);
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
        parent.application().handle_data(*this, read_buf_);
        prepare_next_read();
      }
      return true;
    } else {
      auto err = get<sec>(ret);
      CAF_LOG_DEBUG("receive failed" << CAF_ARG(err));
      parent.application().handle_error(err);
      return false;
    }
  }

  template <class Parent>
  bool handle_write_event(Parent& parent) {
    // Try to write leftover data.
    write_some(parent);
    // Get new data from parent.
    // TODO: dont read all messages at once - get one by one.
    for (auto msg = parent.next_message(); msg != nullptr;
         msg = parent.next_message()) {
      parent.application().write_message(*this, std::move(msg));
    }
    // Write prepared data.
    return write_some(parent);
  }

  template <class Parent>
  bool write_some(Parent& parent) {
    if (write_buf_.empty())
      return false;
    auto len = write_buf_.size() - written_;
    auto buf = write_buf_.data() + written_;
    CAF_LOG_TRACE(CAF_ARG(handle_.id) << CAF_ARG(len));
    auto ret = net::write(handle_, as_bytes(make_span(buf, len)));
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
      parent.application().handle_error(err);
      return false;
    }
    return true;
  }

  template <class Parent>
  void resolve(Parent& parent, const std::string& path, actor listener) {
    parent.application().resolve(parent, path, listener);
  }

  template <class Parent>
  void timeout(Parent& parent, atom_value value, uint64_t id) {
    parent.application().timeout(*this, value, id);
  }

  template <class Application>
  void handle_error(Application& application, sec code) {
    application.handle_error(code);
  }

  void prepare_next_read();

  void configure_read(net::receive_policy::config cfg);

  void write_packet(span<const byte> buf);

private:
  net::stream_socket handle_;

  std::vector<byte> read_buf_;
  std::vector<byte> write_buf_;

  size_t max_consecutive_reads_; // TODO use this field!
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  net::receive_policy_flag rd_flag_;

  size_t written_;
};

} // namespace policy
} // namespace caf
