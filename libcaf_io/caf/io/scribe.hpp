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

#include <vector>

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/io/broker_servant.hpp"
#include "caf/io/network/stream_manager.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/message.hpp"

namespace caf::io {

using scribe_base
  = broker_servant<network::stream_manager, connection_handle, new_data_msg>;

/// Manages a stream.
/// @ingroup Broker
class CAF_IO_EXPORT scribe : public scribe_base {
public:
  scribe(connection_handle conn_hdl);

  ~scribe() override;

  /// Implicitly starts the read loop on first call.
  virtual void configure_read(receive_policy::config config) = 0;

  /// Enables or disables write notifications.
  virtual void ack_writes(bool enable) = 0;

  /// Returns the current output buffer.
  virtual byte_buffer& wr_buf() = 0;

  /// Returns the current input buffer.
  virtual byte_buffer& rd_buf() = 0;

  /// Returns the current size of any input (first) and output (second) buffer
  /// managed by this handler. Always returns `(0, 0)` for event handlers
  /// without buffers.
  virtual std::pair<size_t, size_t> buffer_sizes() const noexcept = 0;

  /// Flushes the output buffer, i.e., sends the
  /// content of the buffer via the network.
  virtual void flush() = 0;

  bool consume(execution_unit*, const void*, size_t) override;

  void data_transferred(execution_unit*, size_t, size_t) override;

protected:
  message detach_message() override;
};

using scribe_ptr = intrusive_ptr<scribe>;

} // namespace caf::io
