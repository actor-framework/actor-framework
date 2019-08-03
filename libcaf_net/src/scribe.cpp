/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/policy/scribe.hpp"

#include <system_error>

#include "caf/config.hpp"

namespace caf {
namespace policy {

scribe::scribe(caf::net::stream_socket handle)
  : handle_(handle),
    max_consecutive_reads_(0),
    read_threshold_(1024),
    collected_(0),
    max_(1024),
    rd_flag_(net::receive_policy_flag::exactly),
    written_(0) {
  // nop
}

void scribe::prepare_next_read() {
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

void scribe::configure_read(net::receive_policy::config cfg) {
  rd_flag_ = cfg.first;
  max_ = cfg.second;
  prepare_next_read();
}

void scribe::write_packet(span<char> buf) {
  write_buf_.insert(write_buf_.end(), buf.begin(), buf.end());
}

} // namespace policy
} // namespace caf
