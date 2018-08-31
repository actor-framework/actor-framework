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

#include "caf/io/network/newb.hpp"

namespace caf {
namespace io {
namespace network {

// -- newb_base ----------------------------------------------------------------

newb_base::newb_base(default_multiplexer& dm, native_socket sockfd)
    : event_handler(dm, sockfd) {
  // nop
}

// -- transport_policy ---------------------------------------------------------

transport_policy::transport_policy()
  : received_bytes{0},
    max_consecutive_reads{50} {
  // nop
}

transport_policy::~transport_policy() {
  // nop
}

io::network::rw_state transport_policy::write_some(newb_base*) {
  return io::network::rw_state::indeterminate;
}

io::network::rw_state transport_policy::read_some(newb_base*) {
  return io::network::rw_state::indeterminate;
}

bool transport_policy::should_deliver() {
  return true;
}

bool transport_policy::must_read_more(newb_base*) {
  return false;
}

void transport_policy::prepare_next_read(newb_base*) {
  // nop
}

void transport_policy::prepare_next_write(newb_base*) {
  // nop
}

void transport_policy::configure_read(receive_policy::config) {
  // nop
}

void transport_policy::flush(newb_base*) {
  // nop
}

byte_buffer& transport_policy::wr_buf() {
  return offline_buffer;
}

expected<native_socket>
transport_policy::connect(const std::string&, uint16_t,
                          optional<io::network::protocol::network>) {
  return sec::bad_function_call;
}

// -- accept_policy ------------------------------------------------------------

accept_policy::~accept_policy() {
  // nop
}

// -- protocol_policy_base -----------------------------------------------------

protocol_policy_base::~protocol_policy_base() {
  // nop
}

} // namespace network
} // namespace io
} // namespace caf
