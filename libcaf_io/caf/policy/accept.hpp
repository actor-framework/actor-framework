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

#include "caf/config.hpp"
#include "caf/expected.hpp"
#include "caf/io/network/native_socket.hpp"
#include "caf/policy/transport.hpp"

namespace caf {
namespace io {

struct newb_base;

template<class Message>
struct newb;

} // namespace io

namespace policy {

template <class Message>
struct accept {
  accept(bool manual_read = false)
      : manual_read(manual_read) {
    // nop
  }

  virtual ~accept() {
    // nop
  }

  virtual expected<io::network::native_socket>
  create_socket(uint16_t port, const char* host, bool reuse = false) = 0;

  virtual std::pair<io::network::native_socket, transport_ptr>
  accept_event(io::network::newb_base*) {
    return {0, nullptr};
  }

  /// If `requires_raw_data` is set to true, the acceptor will only call
  /// this function for new read event and let the policy handle everything
  /// else.
  virtual void read_event(io::network::newb_base*) {
    // nop
  }

  virtual error write_event(io::network::newb_base*) {
    return none;
  }

  virtual void init(io::network::newb_base*, io::newb<Message>&) {
    // nop
  }

  virtual void shutdown(io::network::newb_base*,
                        io::network::native_socket) {
    // nop
  }

  bool manual_read;
};

template <class Message>
using accept_ptr = std::unique_ptr<accept<Message>>;

} // namespace policy
} // namespace caf
