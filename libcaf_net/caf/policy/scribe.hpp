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
  explicit scribe(net::stream_socket handle)
    : handle_(handle),
      max_consecutive_reads_(0),
      read_threshold_(1024),
      collected_(0),
      max_(1024),
      rd_flag_(net::receive_policy_flag::exactly),
      written_(0) {
    // nop
  }

  net::stream_socket handle_;

  net::stream_socket handle() {
    return handle_;
  }

  template <class Parent>
  error init(Parent& parent) {
    prepare_next_read();
    parent.mask_add(net::operation::read_write);
    return none;
  }

  template <class Parent>
  bool handle_read_event(Parent& parent) {
    void* buf = read_buf_.data() + collected_;
    size_t len = read_threshold_ - collected_;
    CAF_LOG_TRACE(CAF_ARG(handle_.id) << CAF_ARG(len));
    auto rres = read(handle_, buf, len);
    if (rres.is<caf::sec>()) {
      // Make sure WSAGetLastError gets called immediately on Windows.
      CAF_LOG_DEBUG("receive failed" << CAF_ARG(get<sec>(rres)));
      handle_error(parent, get<caf::sec>(rres));
      return false;
    }

    CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id) << CAF_ARG(rres));
    auto result = (get<size_t>(rres) > 0)
                    ? static_cast<size_t>(get<size_t>(rres))
                    : 0;
    collected_ += result;
    if (collected_ >= read_threshold_) {
      parent.application().process(read_buf_, *this, parent);
      prepare_next_read();
      return false;
    } else {
      return true;
    }
  }

  template <class Parent>
  bool handle_write_event(Parent& parent) {
    for (auto msg = parent.next_message(); msg != nullptr; msg = parent.next_message()) {
      parent.application().prepare(std::move(msg), *this);
    }

    // write prepared data
    if (write_buf_.empty())
      return false;
    auto len = write_buf_.size() - written_;
    void* buf = write_buf_.data() + written_;
    CAF_LOG_TRACE(CAF_ARG(handle_.id) << CAF_ARG(len));
    auto sres = net::write(handle_, buf, len);
    if (sres.is<caf::sec>()) {
      CAF_LOG_ERROR("send failed");
      handle_error(parent, get<caf::sec>(sres));
      return false;
    }
    CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id)
                                      << CAF_ARG(sres));
    auto result = (get<size_t>(sres) > 0)
                  ? static_cast<size_t>(get<size_t>(sres))
                  : 0;

    // update state
    written_ += result;
    if (written_ >= write_buf_.size()) {
      written_ = 0;
      write_buf_.clear();
      return false;
    } else {
      return true;
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

  std::vector<char>& wr_buf();

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
