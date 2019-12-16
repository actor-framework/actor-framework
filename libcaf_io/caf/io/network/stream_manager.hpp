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

#include <cstddef>

#include "caf/detail/io_export.hpp"
#include "caf/io/network/manager.hpp"

namespace caf::io::network {

/// A stream manager configures an IO stream and provides callbacks
/// for incoming data as well as for error handling.
class CAF_IO_EXPORT stream_manager : public manager {
public:
  ~stream_manager() override;

  /// Called by the underlying I/O device whenever it received data.
  /// @returns `true` if the manager accepts further reads, otherwise `false`.
  virtual bool consume(execution_unit* ctx, const void* buf, size_t bsize) = 0;

  /// Called by the underlying I/O device whenever it sent data.
  virtual void data_transferred(execution_unit* ctx, size_t num_bytes,
                                size_t remaining_bytes)
    = 0;

  /// Get the port of the underlying I/O device.
  virtual uint16_t port() const = 0;

  /// Get the address of the underlying I/O device.
  virtual std::string addr() const = 0;
};

} // namespace caf::io::network
