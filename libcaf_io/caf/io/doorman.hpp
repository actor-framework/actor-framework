// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/accept_handle.hpp"
#include "caf/io/broker_servant.hpp"
#include "caf/io/network/acceptor_manager.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/detail/io_export.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"

#include <cstddef>
#include <cstdint>

namespace caf::io {

using doorman_base = broker_servant<network::acceptor_manager, accept_handle,
                                    new_connection_msg>;

/// Manages incoming connections.
/// @ingroup Broker
class CAF_IO_EXPORT doorman : public doorman_base {
public:
  doorman(accept_handle acc_hdl);

  ~doorman() override;

  using doorman_base::new_connection;

  bool new_connection(execution_unit* ctx, connection_handle x);

  /// Starts listening on the selected port.
  virtual void launch() = 0;

protected:
  message detach_message() override;
};

using doorman_ptr = intrusive_ptr<doorman>;

} // namespace caf::io
