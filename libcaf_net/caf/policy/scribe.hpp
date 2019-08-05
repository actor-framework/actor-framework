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

#include <caf/error.hpp>
#include <caf/fwd.hpp>
#include <caf/logger.hpp>
#include <caf/net/endpoint_manager.hpp>
#include <caf/net/receive_policy.hpp>
#include <caf/net/stream_socket.hpp>
#include <caf/sec.hpp>
#include <caf/span.hpp>
#include <caf/variant.hpp>

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#else
#  include <caf/actor.hpp>
#  include <sys/socket.h>
#  include <sys/types.h>
#endif

namespace caf {
namespace policy {

class scribe {
public:
  explicit scribe(net::stream_socket handle);

  net::stream_socket handle_;

  net::stream_socket handle() {
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
    void* buf = read_buf_.data() + collected_;
    size_t len = read_threshold_ - collected_;
    CAF_LOG_TRACE(CAF_ARG(handle_.id) << CAF_ARG(len));
    variant<size_t, sec> ret = read(handle_, buf, len);
    if (auto num_bytes = get_if<size_t>(&ret)) {
      CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id) << CAF_ARG(*num_bytes));
      collected_ += *num_bytes;
      if (collected_ >= read_threshold_) {
        parent.application().handle_data(*this, read_buf_);
        prepare_next_read();
      }
      return true;
    } else {
      // Make sure WSAGetLastError gets called immediately on Windows.
      auto err = get<sec>(ret);
      CAF_LOG_DEBUG("receive failed" << CAF_ARG(err));
      handle_error(parent, err);
      return false;
    }
  }

  template <class Parent>
  bool handle_write_event(Parent& parent) {
    // try to write leftover data!
    write_some(parent);
    // get new data from parent
    for (auto msg = parent.next_message(); msg != nullptr;
         msg = parent.next_message()) {
      parent.application().write_message(*this, std::move(msg));
    }
    // write prepared data
    return write_some(parent);
  }

  template <class Parent>
  bool write_some(Parent& parent) {
    if (write_buf_.empty())
      return false;
    auto len = write_buf_.size() - written_;
    void* buf = write_buf_.data() + written_;
    CAF_LOG_TRACE(CAF_ARG(handle_.id) << CAF_ARG(len));
    auto ret = net::write(handle_, buf, len);
    if (auto num_bytes = get_if<size_t>(&ret)) {
      CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id) << CAF_ARG(*num_bytes));
      // update state
      written_ += *num_bytes;
      if (written_ >= write_buf_.size()) {
        written_ = 0;
        write_buf_.clear();
        return false;
      } else {
        return true;
      }
    } else {
      CAF_LOG_ERROR("send failed");
      handle_error(parent, get<sec>(ret));
      return false;
    }
  }

  template <class Parent>
  void resolve(Parent& parent, const std::string& path, actor listener) {
    parent.application().resolve(parent, path, listener);
    // TODO should parent be passed as well?
  }

  template <class Parent>
  void timeout(Parent& parent, atom_value value, uint64_t id) {
    parent.application().timeout(*this, value, id);
    // TODO should parent be passed as well?
  }

  template <class Application>
  void handle_error(Application& application, sec code) {
    application.handle_error(code);
  }

  void prepare_next_read();

  void configure_read(net::receive_policy::config cfg);

  void write_packet(span<char> buf);

private:
  std::vector<char> read_buf_;
  std::vector<char> write_buf_;

  size_t max_consecutive_reads_;
  size_t read_threshold_;
  size_t collected_;
  size_t max_;
  net::receive_policy_flag rd_flag_;

  size_t written_;
};

} // namespace policy
} // namespace caf
