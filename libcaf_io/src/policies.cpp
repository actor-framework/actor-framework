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

#include "caf/policy/accept.hpp"
#include "caf/policy/protocol.hpp"
#include "caf/policy/transport.hpp"

namespace caf {
namespace policy {

// -- transport_policy ---------------------------------------------------------

transport::transport()
  : received_bytes{0},
    max_consecutive_reads{50} {
  // nop
}

transport::~transport() {
  // nop
}

io::network::rw_state transport::write_some(io::newb_base*) {
  return io::network::rw_state::indeterminate;
}

io::network::rw_state transport::read_some(io::newb_base*) {
  return io::network::rw_state::indeterminate;
}

bool transport::should_deliver() {
  return true;
}

bool transport::must_read_more(io::newb_base*) {
  return false;
}

void transport::prepare_next_read(io::newb_base*) {
  // nop
}

void transport::prepare_next_write(io::newb_base*) {
  // nop
}

void transport::configure_read(io::receive_policy::config) {
  // nop
}

void transport::flush(io::newb_base*) {
  // nop
}

byte_buffer& transport::wr_buf() {
  return offline_buffer;
}

expected<io::network::native_socket>
transport::connect(const std::string&, uint16_t,
                          optional<io::network::protocol::network>) {
  return sec::bad_function_call;
}

// -- accept_policy ------------------------------------------------------------

accept::~accept() {
  // nop
}

// -- protocol_policy_base -----------------------------------------------------

protocol_base::~protocol_base() {
  // nop
}

} // namespace policy
} // namespace caf
