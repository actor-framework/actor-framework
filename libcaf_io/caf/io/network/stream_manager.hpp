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

#ifndef CAF_IO_NETWORK_STREAM_MANAGER_HPP
#define CAF_IO_NETWORK_STREAM_MANAGER_HPP

#include <cstddef>

#include "caf/io/network/manager.hpp"

namespace caf {
namespace io {
namespace network {

/// A stream manager configures an IO stream and provides callbacks
/// for incoming data as well as for error handling.
class stream_manager : public manager {
public:
  ~stream_manager();

  /// Called by the underlying IO device whenever it received data.
  virtual void consume(const void* data, size_t num_bytes) = 0;
};

} // namespace network
} // namespace io
} // namespace caf

#endif // CAF_IO_NETWORK_STREAM_MANAGER_HPP
