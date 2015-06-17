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

#include "caf/io/network/multiplexer.hpp"
#include "caf/io/network/default_multiplexer.hpp"

namespace caf {
namespace io {
namespace network {

multiplexer::~multiplexer() {
  CAF_LOG_TRACE("");
}

boost::asio::io_service* pimpl() {
  return nullptr;
}

multiplexer_ptr multiplexer::make() {
  CAF_LOGF_TRACE("");
  return multiplexer_ptr{new default_multiplexer};
}

boost::asio::io_service* multiplexer::pimpl() {
  return nullptr;
}

multiplexer::supervisor::~supervisor() {
  // nop
}

multiplexer::runnable::~runnable() {
  // nop
}

} // namespace network
} // namespace io
} // namespace caf
