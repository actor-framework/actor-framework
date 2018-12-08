/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         * | |___ / ___ \|  _|      Framework                     *
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

#include "caf/config.hpp"

#include "caf/error.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/newb_base.hpp"
#include "caf/io/network/protocol.hpp"
#include "caf/io/network/rw_state.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/logger.hpp"
#include "caf/policy/protocol.hpp"
#include "caf/sec.hpp"

namespace caf {
namespace policy {

struct transport {
  transport();

  virtual ~transport();

  virtual io::network::rw_state write_some(io::network::newb_base*);

  virtual io::network::rw_state read_some(io::network::newb_base*);

  virtual bool should_deliver();

  virtual bool must_read_more(io::network::newb_base*);

  virtual void prepare_next_read(io::network::newb_base*);

  virtual void prepare_next_write(io::network::newb_base*);

  virtual void configure_read(io::receive_policy::config);

  virtual void flush(io::network::newb_base*);

  virtual byte_buffer& wr_buf();

  template <class T>
  error read_some(io::network::newb_base* parent, protocol<T>& policy) {
    using io::network::rw_state;
    CAF_LOG_TRACE("");
    size_t reads = 0;
    while (reads < max_consecutive_reads || must_read_more(parent)) {
      auto read_result = read_some(parent);
      switch (read_result) {
        case rw_state::success:
          if (received_bytes == 0)
            return none;
          if (should_deliver()) {
            auto res = policy.read(receive_buffer.data(), received_bytes);
            prepare_next_read(parent);
            if (res)
              return res;
          }
          break;
        case rw_state::indeterminate:
          // No error, but don't continue reading.
          return none;
        case rw_state::failure:
          // Reading failed.
          return sec::runtime_error;
      }
      ++reads;
    }
    return none;
  }

  virtual expected<io::network::native_socket>
  connect(const std::string&, uint16_t,
          optional<io::network::protocol::network> = none);

  virtual void shutdown(io::network::newb_base* parent,
                        io::network::native_socket sockfd);

  size_t received_bytes;
  size_t max_consecutive_reads;

  byte_buffer offline_buffer;
  byte_buffer receive_buffer;
  byte_buffer send_buffer;
};

using transport_ptr = std::unique_ptr<transport>;

} // namespace policy
} // namespace caf
