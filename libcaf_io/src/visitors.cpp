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

#include "caf/io/visitors.hpp"

namespace caf {
namespace io {

purge_visitor::result_type purge_visitor::operator()(const connection_handle& h) {
  auto i = state->tcp_ctx.find(h);
  if (i != state->tcp_ctx.end()) {
    auto& ref = i->second;
    if (ref.callback) {
      CAF_LOG_DEBUG("connection closed during handshake");
      ref.callback->deliver(sec::disconnect_during_handshake);
    }
    state->tcp_ctx.erase(i);
  }
}

purge_visitor::result_type
purge_visitor::operator()(const dgram_scribe_handle& h) {
  auto i = state->udp_ctx.find(h);
  if (i != state->udp_ctx.end()) {
    auto& ref = i->second;
    if (ref.callback) {
      CAF_LOG_DEBUG("connection closed during handshake");
      ref.callback->deliver(sec::disconnect_during_handshake);
    }
    state->udp_ctx.erase(i);
  }
}

} // namespace io
} // namespace caf

