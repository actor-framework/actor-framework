/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifdef CAF_USE_ASIO
# include "caf/io/network/asio_multiplexer.hpp"
  using caf_multiplexer_impl = caf::io::network::asio_multiplexer;
#else
# include "caf/io/network/default_multiplexer.hpp"
  using caf_multiplexer_impl = caf::io::network::default_multiplexer;
#endif

namespace caf {
namespace io {
namespace network {

multiplexer::~multiplexer() {
  // nop
}

boost::asio::io_service* pimpl() {
  return nullptr;
}

multiplexer_ptr multiplexer::make() {
  return multiplexer_ptr{new caf_multiplexer_impl};
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
