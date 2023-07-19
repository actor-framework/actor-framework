// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/broker_servant.hpp"
#include "caf/io/network/stream_manager.hpp"
#include "caf/io/receive_policy.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/message.hpp"

#include <vector>

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
