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

#include <caf/fwd.hpp>
#include <caf/net/stream_socket.hpp>
#include <caf/net/receive_policy.hpp>
#include <caf/error.hpp>
#include <caf/logger.hpp>
#include <caf/sec.hpp>
#include <caf/variant.hpp>

#ifdef CAF_WINDOWS
# include <winsock2.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
#include <caf/actor.hpp>

#endif


namespace caf {
namespace policy {

class scribe_policy {
public:
  scribe_policy(net::stream_socket handle) : handle_(handle) {
    // nop
  }

  net::stream_socket handle_;

  net::stream_socket handle() {
    return handle_;
  }

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  template <class Parent>
  bool handle_read_event(Parent& parent) {
    CAF_LOG_TRACE(CAF_ARG(handle_.id()) << CAF_ARG(len));
    void* buf = read_buf_.data() + collected_;
    size_t len = read_threshold_ - collected_;
    auto rres = read(handle_, buf, len);
    if (rres.is<caf::sec>()) {
      // Make sure WSAGetLastError gets called immediately on Windows.
      handle_error(parent, get<caf::sec>(rres));
      return false;
    }

    CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id) << CAF_ARG(rres));
    auto result = (get<size_t>(rres) > 0) ?
                  static_cast<size_t>(get<size_t>(rres)) : 0;
    collected_ += result;
    if (collected_ >= read_threshold_) {
      collected_ = 0;
      return false;
    } else {
      return true;
    }
  }

  template <class Parent>
  bool handle_write_event(Parent& parent) {
    if (!write_buf_.empty())
      while(write_some(parent)); // write while write_buf not empty
    
    // check for new messages in parents message_queue
    auto& msg_queue = parent.message_queue();
    while (!msg_queue.empty()) {
      // TODO: would be nice to have some kind of `get_next_elem()` function
      auto& elem = msg_queue.dequeue();
      // TODO parameter list from proposal -> pass to application
      parent->application().prepare(elem, *this, parent);
    }
    // write prepared data
    return write_some(parent);
  }

  template <class Parent>
  bool write_some(Parent& parent) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(len));
    auto len = write_buf_.size() - written_;
    void* buf = write_buf_.data() + written_;
    auto sres = net::write(handle_, buf, len);
    if (sres.is<caf::sec>()) {
      CAF_LOG_ERROR("send failed");
      handle_error(parent, get<caf::sec>(sres));
      return false;
    }
    CAF_LOG_DEBUG(CAF_ARG(len) << CAF_ARG(handle_.id) << CAF_ARG(sres));
    auto result = (get<size_t>(sres) > 0) ? static_cast<size_t>(get<size_t>(sres)) : 0;

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
    parent->application().resolve(*this, path, listener);
    // TODO should parent be passed as well?
  }

  template <class Parent>
  void timeout(Parent& parent, atom_value value, uint64_t id) {
    parent->application().timeout(*this, value, id);
    // TODO should parent be passed as well?
  }

  template <class Parent>
  void handle_error(Parent& parent, sec code) {
    parent.application().handle_error(code);
  }

  void prepare_next_read() {
    collected_ = 0;
    // This cast does nothing, but prevents a weird compiler error on GCC <= 4.9.
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

  void configure_read(net::receive_policy::config cfg) {
    rd_flag_ = cfg.first;
    max_ = cfg.second;
    prepare_next_read();
  }

  std::vector<char>& wr_buf() {
    return write_buf_;
  }


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
