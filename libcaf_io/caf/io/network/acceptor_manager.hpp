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

#ifndef CAF_IO_NETWORK_ACCEPTOR_MANAGER_HPP
#define CAF_IO_NETWORK_ACCEPTOR_MANAGER_HPP

#include "caf/io/network/manager.hpp"

namespace caf {
namespace io {
namespace network {

/// An acceptor manager configures an acceptor and provides
/// callbacks for incoming connections as well as for error handling.
class acceptor_manager : public manager {
public:
  ~acceptor_manager();

  /// Called by the underlying IO device to indicate that
  /// a new connection is awaiting acceptance.
  virtual void new_connection() = 0;
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_ACCEPTOR_MANAGER_HPP
