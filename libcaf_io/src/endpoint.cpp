/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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


#include "caf/io/endpoint.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace io {

endpoint::endpoint(abstract_broker* parent, datagram_sink_handle hdl)
    : endpoint_base(parent, hdl) {
  // nop
}

endpoint::~endpoint() {
  CAF_LOG_TRACE("");
}

message endpoint::detach_message() {
  return make_message(endpoint_closed_msg{hdl()});
}

} // namespace io
} // namespace caf
