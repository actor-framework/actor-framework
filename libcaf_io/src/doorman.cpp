/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/doorman.hpp"

#include "caf/detail/logging.hpp"

#include "caf/io/abstract_broker.hpp"

namespace caf {
namespace io {

doorman::doorman(abstract_broker* ptr, accept_handle acc_hdl, uint16_t p)
    : network::acceptor_manager(ptr),
      hdl_(acc_hdl),
      accept_msg_(make_message(new_connection_msg{hdl_, connection_handle{}})),
      port_(p) {
  // nop
}

doorman::~doorman() {
  // nop
}

void doorman::detach_from_parent() {
  CAF_LOG_TRACE("hdl = " << hdl().id());
  parent()->doormen_.erase(hdl());
}

message doorman::detach_message() {
  return make_message(acceptor_closed_msg{hdl()});
}

void doorman::io_failure(network::operation op) {
  CAF_LOG_TRACE("id = " << hdl().id() << ", "
                        << CAF_TARG(op, static_cast<int>));
  // keep compiler happy when compiling w/o logging
  static_cast<void>(op);
  detach(true);
}

} // namespace io
} // namespace caf
