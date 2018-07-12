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

#include "caf/io/fwd.hpp"
#include "caf/io/scribe.hpp"

#include "caf/io/network/stream_impl.hpp"
#include "caf/io/network/native_socket.hpp"

#include "caf/policy/tcp.hpp"

namespace caf {
namespace io {
namespace network {

/// Default scribe implementation.
class scribe_impl : public scribe {
public:
  scribe_impl(default_multiplexer& mx, native_socket sockfd);

  void configure_read(receive_policy::config config) override;

  void ack_writes(bool enable) override;

  std::vector<char>& wr_buf() override;

  std::vector<char>& rd_buf() override;

  void stop_reading() override;

  void flush() override;

  std::string addr() const override;

  uint16_t port() const override;

  void launch();

  void add_to_loop() override;

  void remove_from_loop() override;

protected:
  bool launched_;
  stream_impl<policy::tcp> stream_;
};

} // namespace network
} // namespace io
} // namespace caf
